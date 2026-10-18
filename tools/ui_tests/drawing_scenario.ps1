# Dessin BIM dans l'application réelle (UI Automation, vrais clics dans la vue 3D, vrais dialogues) :
#   1. nouveau projet vierge (créé dans <Work>) ;
#   2. Dessiner Poutre : 1er clic puis Échap → aucun nœud (pas de nœud abandonné) ;
#   3. poutre par deux clics → 2 nœuds, 1 élément ; Annuler → modèle vide ; Rétablir ;
#   4. Dessiner Dalle : 3 clics sans nœud créé, Entrée → dalle et ses 3 nœuds ;
#   5. dalle croisée (nœud papillon) : refus motivé, rien de créé ;
#   6. voile à base inclinée saisi dans le dialogue « Surface » : créé (parallélogramme), avertissement « Base inclinée » ;
#   7. Paramètres de modélisation : onglets Fondation et Treillis ;
#   8. Portique (outil de la vue) : créé, puis relancé sur les mêmes pieds → refusé (console), rien de créé ;
#   9. palette : identité BIM et quantités de la dalle sélectionnée (clic au centre de la dalle) ;
#  10. arbre du modèle : branche « Structure BIM (IFC) », produit « Poteau C1 » sous son étage, clic → propriétés du
#      poteau (la dalle était sélectionnée : le changement de panneau prouve l'effet du clic) ;
#  11. types de produits : « Créer les types de produits depuis le modèle » (clic droit sur la branche BIM, menu au
#      clavier), message de la console, branche « Types de produits ».
# Garde-fous : l'essai s'arrête (sans rien enregistrer) si la fenêtre active n'est pas celle du projet qu'il a créé ;
# aucun clic n'est envoyé ailleurs (un clic dans le Start Center ouvrirait un projet récent). Les points de la vue
# sont placés par rapport à son centre (indépendants de la disposition des panneaux) ; accrochages coupés pendant
# l'essai. Captures dans <Work>\shots, journal dans <Work>\drawing_scenario.log ; réglages
# HKCU\Software\TSA Engineering sauvegardés puis restaurés (après la fermeture de TSA).
param([string]$Exe = "", [Parameter(Mandatory = $true)][string]$Work)
if (-not $Exe) { $Exe = Join-Path $PSScriptRoot "..\..\build-ninja-release\TSA.exe" }
$ErrorActionPreference = "Continue"
. (Join-Path $PSScriptRoot "uia_lib.ps1")
$OutDir = Join-Path $Work "shots"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$Log = Join-Path $Work "drawing_scenario.log"
"" | Set-Content $Log -Encoding UTF8
# Write-Host et non Write-Output : dans une fonction, la sortie du pipeline deviendrait sa valeur de retour
function Say($text) { Write-Host $text; Add-Content $Log $text -Encoding UTF8 }
$script:results = @()
function Check($label, [bool]$ok, $observed) {
    $script:results += [pscustomobject]@{ Step = $label; Ok = $ok }
    Say ("    [{0}] {1} — observé : {2}" -f $(if ($ok) { "OK" } else { "ÉCHEC" }), $label, $observed)
}

$RegBackup = Join-Path $Work "tsa_settings.reg"
& reg.exe export "HKCU\Software\TSA Engineering" $RegBackup /y *> $null
$script:projTitle = $null
$script:lastGood = @(-1, -1)

function Park { $s = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds; [MouseInput]::SetCursorPos($s.Width - 2, $s.Height - 2) | Out-Null; Start-Sleep -Milliseconds 400 }
function Shot($name) { Park; Capture $script:w (Join-Path $OutDir $name) | Out-Null; Say "    capture : shots\$name" }
function Texts($pattern, $root = $null) {
    if (-not $root) { $root = Fresh $script:w }
    $out = @(); foreach ($t in (Find-All $root $CT::Text)) { $n = $t.Current.Name; if ($n -match $pattern) { $out += $n } }
    return ($out | Select-Object -Unique)
}
function PromptText($pattern) { (Texts $pattern) -join " | " }
function RibbonButton($pattern) {
    foreach ($type in $CT::Button, $CT::CheckBox) {
        foreach ($b in (Find-All (Fresh $script:w) $type)) { if ($b.Current.Name -like $pattern -and -not $b.Current.IsOffscreen) { return $b } }
    }
    return $null
}
function TitleBarButton($name) {
    # Barre d'outils de la barre de titre (Annuler / Rétablir de l'application, pas ceux d'un dialogue)
    Find-All (Fresh $script:w) $CT::Button | Where-Object { $_.Current.Name -eq $name -and $_.Current.BoundingRectangle.Top -lt 60 } | Select-Object -First 1
}
function RibbonTab($name) {
    $t = Find-All (Fresh $script:w) $CT::TabItem | Where-Object { $_.Current.Name -eq $name } | Select-Object -First 1
    if ($t) { Click-Mouse $t; Start-Sleep -Seconds 1 }
}
function EnsureOn($pattern) {
    $b = RibbonButton $pattern
    if ($b) { try { if ((Toggle-State $b) -ne "On") { Click-Mouse $b } } catch {} ; Say "    $($b.Current.Name) : $(try { Toggle-State $b } catch { '?' })" }
}
function EnsureOff($pattern) {
    $b = RibbonButton $pattern
    if ($b) { try { if ((Toggle-State $b) -eq "On") { Click-Mouse $b } } catch {} ; Say "    $($b.Current.Name) : $(try { Toggle-State $b } catch { '?' })" }
}
function Counts {
    # Barre d'état : « 3 nœuds · 1 élém. · 0 sél. » ; relue jusqu'à une valeur valide (arbre UIA momentanément vide
    # à l'ouverture d'un dialogue)
    FocusMain   # un dialogue non modal actif masque l'arbre UIA de la fenêtre principale
    foreach ($try in 1..25) {
        $t = @(Texts "^\d+ nœuds · \d+ élém") | Select-Object -First 1
        if ($t -match "^(\d+) nœuds · (\d+) élém") { $script:lastGood = @([int]$Matches[1], [int]$Matches[2]); return $script:lastGood }
        Start-Sleep -Milliseconds 400
    }
    return @(-1, -1)
}
function Base {
    # Référence d'une étape : juste après l'ouverture ou la fermeture d'un dialogue, l'arbre UIA de la fenêtre principale
    # reste illisible ; aucune modification n'a lieu entre deux étapes, la dernière lecture valide fait foi
    $c = Counts
    if ($c[0] -lt 0) { Say "    référence : dernière lecture valide $($script:lastGood -join '/')"; return $script:lastGood }
    return $c
}
function CountsText { $c = Counts; "$($c[0]) nœud(s), $($c[1]) élément(s)" }
function Delta($before) {
    $c = Counts
    Say "    compteurs : avant $($before -join '/') ; après $($c -join '/')"
    if ($c[0] -lt 0 -or $before[0] -lt 0) { return @(999, 999) }
    return @(($c[0] - $before[0]), ($c[1] - $before[1]))
}
function FocusMain { [Win]::SetForegroundWindow($script:w) | Out-Null; Start-Sleep -Milliseconds 300 }
function ChildWindow($titlePattern, $seconds = 10) {
    # Fenêtre de premier niveau ou fille directe (Find-Dialog), sinon à toute profondeur : un message ouvert par un
    # dialogue est petit-enfant de la fenêtre principale
    $deadline = (Get-Date).AddSeconds($seconds)
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $script:p.Id)
    $winCond = New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $CT::Window)
    while ((Get-Date) -lt $deadline) {
        $found = Find-Dialog $script:p.Id $titlePattern 1
        if ($found) { return $found }
        try {
            foreach ($top in $AE::RootElement.FindAll($TS::Children, $cond)) {
                foreach ($win in $top.FindAll($TS::Descendants, $winCond)) { if ($win.Current.Name -like $titlePattern) { return $win } }
            }
        } catch {}
    }
    return $null
}
function ConsoleText($pattern) {
    # Console « Console / Diagnostics » (QTextEdit) : lue par TextPattern
    foreach ($type in $CT::Document, $CT::Edit) {
        foreach ($e in (Find-All (Fresh $script:w) $type)) {
            try {
                $txt = $e.GetCurrentPattern([System.Windows.Automation.TextPattern]::Pattern).DocumentRange.GetText(-1)
                if ($txt -match $pattern) { return (($txt -split "`n") | Where-Object { $_ -match $pattern }) -join " / " }
            } catch {}
        }
    }
    return ""
}
function DismissDialogs {
    # Boîtes restées ouvertes (message, couleur, préréglages…) : Annuler / Fermer / OK, jamais Enregistrer
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $script:p.Id)
    $winCond = New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $CT::Window)
    foreach ($round in 1..5) {
        $acted = $false
        foreach ($top in $AE::RootElement.FindAll($TS::Children, $cond)) {
            foreach ($win in $top.FindAll($TS::Descendants, $winCond)) {
                foreach ($label in "Annuler", "Fermer", "OK") {
                    $b = Find-ByName $win $label $CT::Button
                    if ($b) { Say "    boîte « $($win.Current.Name) » refermée ($label)"; Click $b; $acted = $true; break }
                }
                if ($acted) { break }
            }
            if ($acted) { break }
        }
        if (-not $acted) { return }
        Start-Sleep -Milliseconds 800
    }
}
function Finish([int]$code) {
    DismissDialogs
    $closed = Close-App $script:p $script:w 60 $false
    if (-not $closed) { DismissDialogs; $closed = Close-App $script:p $script:w 30 $false }
    $script:p.Refresh()
    if (-not $script:p.HasExited -and $script:projTitle -and ($script:p.MainWindowTitle -replace ' \*$', '') -eq $script:projTitle) {
        Say "    fermeture forcée du projet d'essai (non enregistré)"; Stop-Process -Id $script:p.Id -Force; $closed = $true
    }
    try { $script:p.WaitForExit(15000) | Out-Null } catch {}
    $ok = @($script:results | Where-Object { $_.Ok }).Count
    Say "fin : application fermée = $closed ; $ok / $($script:results.Count) vérifications réussies"
    if ($script:p.HasExited -and (Test-Path $RegBackup)) { & reg.exe delete "HKCU\Software\TSA Engineering" /f *> $null; & reg.exe import $RegBackup *> $null }
    elseif (-not $script:p.HasExited) { Say "    ATTENTION : TSA encore ouvert, réglages NON restaurés (sauvegarde : $RegBackup)" }
    exit $code
}
function Guard {
    # Avant tout clic dans la vue : la fenêtre doit être celle du projet créé par l'essai
    $script:p.Refresh()
    if ($script:p.HasExited -or ($script:p.MainWindowTitle -replace ' \*$', '') -ne $script:projTitle) {
        Say "ARRÊT : fenêtre « $($script:p.MainWindowTitle) » au lieu de « $script:projTitle » — aucun clic envoyé"
        Finish 2
    }
}
function ViewRect {
    foreach ($e in (Find-All (Fresh $script:w) $CT::Pane) + (Find-All (Fresh $script:w) $CT::Custom) + (Find-All (Fresh $script:w) $CT::Group)) {
        if ($e.Current.ClassName -eq "OccView") { return $e.Current.BoundingRectangle }
    }
    return $null
}
# Points de la vue : décalage (pixels pour une vue de 700 px de haut) par rapport à son centre. Vue par défaut d'un
# projet neuf : centre du plan de travail à (-63, +38), demi-diagonales 390 × 225 ; tous les points sont sur le plan
# (Z = 0).
function ViewClick($dx, $dy) {
    Guard
    $r = ViewRect
    if (-not $r) { Say "ARRÊT : vue 3D (OccView) introuvable"; Finish 2 }
    $k = $r.Height / 700.0
    $x = [int]($r.Left + $r.Width / 2 + $dx * $k); $y = [int]($r.Top + $r.Height / 2 + $dy * $k)
    [MouseInput]::Click($x, $y); Start-Sleep -Milliseconds 700
}
function ViewHoverClick($dx, $dy) {
    # Sélection : OCCT détecte l'objet au survol (MoveTo) ; survol avant le clic
    Guard
    $r = ViewRect
    $k = $r.Height / 700.0
    $x = [int]($r.Left + $r.Width / 2 + $dx * $k); $y = [int]($r.Top + $r.Height / 2 + $dy * $k)
    [MouseInput]::SetCursorPos($x - 3, $y) | Out-Null; Start-Sleep -Milliseconds 250
    [MouseInput]::SetCursorPos($x, $y) | Out-Null; Start-Sleep -Milliseconds 500
    [MouseInput]::Click($x, $y); Start-Sleep -Milliseconds 800
}
function UndoButtonInfo($name) { $b = TitleBarButton $name; if ($b) { "« $($b.Current.Name) » actif=$($b.Current.IsEnabled)" } else { "introuvable" } }

# --- 1. Nouveau projet vierge ---------------------------------------------------------------------------------------
$script:p = Start-Process -FilePath $Exe -PassThru
$deadline = (Get-Date).AddSeconds(120)
while ((Get-Date) -lt $deadline) { $script:p.Refresh(); if ($script:p.MainWindowHandle -ne [IntPtr]::Zero -and $script:p.MainWindowTitle -like "TSA*") { break }; Start-Sleep -Milliseconds 500 }
$script:w = $script:p.MainWindowHandle
Start-Sleep -Seconds 8
$wa = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
[Win]::ShowWindow($script:w, 9) | Out-Null
[Win]::MoveWindow($script:w, $wa.Left, $wa.Top, $wa.Width, $wa.Height, $true) | Out-Null
Start-Sleep -Seconds 2
# Le grand bouton du Start Center n'apparaît dans l'arbre UIA qu'après quelques secondes (l'icône de la barre
# d'outils, de même nom, ne réagit pas au clic simulé)
$np = $null
$deadline = (Get-Date).AddSeconds(40)
while (-not $np -and (Get-Date) -lt $deadline) {
    $np = @(Find-All (Fresh $script:w) $CT::Button | Where-Object { $_.Current.Name -eq "Nouveau projet" -and $_.Current.BoundingRectangle.Width -gt 200 })[0]
    if (-not $np) { Start-Sleep -Milliseconds 500 }
}
if (-not $np) { Say "ARRÊT : bouton « Nouveau projet » du Start Center introuvable"; Finish 2 }
# Le premier clic peut ne servir qu'à activer la fenêtre : jusqu'à 3 tentatives (clic souris, puis InvokePattern)
$nd = $null
foreach ($attempt in 1..3) {
    FocusMain
    if ($attempt -eq 1) { Click-Mouse $np } else { Click $np }
    $nd = ChildWindow "Nouveau projet" 5
    if ($nd) { break }
    Say "    boîte « Nouveau projet » absente après la tentative $attempt"
}
if (-not $nd) { Shot "00_arret_nouveau_projet.png"; Say "ARRÊT : boîte « Nouveau projet » introuvable"; Finish 2 }
$projName = ""
foreach ($e in @(Find-All $nd $CT::Edit)) {
    if ((Value-Of $e) -like "*:\*") { Set-Value $e (Join-Path $Work "projet") } elseif ($e.Current.Name -like "Nom*") { $projName = Value-Of $e }
}
Click (Find-ByName (Fresh $nd) "Créer" $CT::Button)
Start-Sleep -Seconds 10
$script:p.Refresh(); $script:w = $script:p.MainWindowHandle
[Win]::MoveWindow($script:w, $wa.Left, $wa.Top, $wa.Width, $wa.Height, $true) | Out-Null
Start-Sleep -Seconds 2
$c = Counts
$title = $script:p.MainWindowTitle
$fileOk = Test-Path (Join-Path (Join-Path $Work "projet") "$projName.tsa")
Say "1. nouveau projet : « $title » (nom saisi « $projName », dossier $(Join-Path $Work 'projet'))"
$isOurs = $projName -and $title -eq "TSA — $projName.tsa" -and $fileOk
Check "projet vierge créé dans le dossier d'essai" ($isOurs -and $c[0] -eq 0 -and $c[1] -eq 0) "$(CountsText) ; fichier présent = $fileOk"
if (-not $isOurs) { Say "ARRÊT : le projet d'essai n'est pas la fenêtre active"; Finish 2 }
$script:projTitle = $title
Say "    vue 3D : $(ViewRect)"
Shot "01_projet_vierge.png"

# Accrochages coupés (points reproductibles) ; console (refus des outils) et propriétés (palette) affichées AVANT
# tout tracé : la disposition ne change plus ensuite, la scène reste au même endroit de l'écran
RibbonTab "Affichage"
EnsureOff "Magnétisme Grille*"
EnsureOff "Accrochage Objets*"
EnsureOn "CONSOLE*DIAGNOSTICS"
EnsureOn "PROPRIÉTÉS"
RibbonTab "Modèle"
Say "    vue 3D après réglages : $(ViewRect)"

# --- 2. Poutre : 1er clic puis Échap ---------------------------------------------------------------------------------
Click-Mouse (RibbonButton "Dessiner*Poutre")
Start-Sleep -Seconds 2
ViewClick -213 -12
$c = Counts
Check "1er point de poutre : aucun nœud créé" ($c[0] -eq 0) (CountsText)
Shot "02a_premier_point.png"
FocusMain; Press "{ESC}"; Start-Sleep -Milliseconds 800
$c = Counts
Check "Échap pendant le tracé : modèle vide" ($c[0] -eq 0 -and $c[1] -eq 0) (CountsText)

# --- 3. Poutre par deux clics, Annuler, Rétablir ---------------------------------------------------------------------
RibbonTab "Modèle"
Click-Mouse (RibbonButton "Dessiner*Poutre")   # Échap a pu terminer la commande : la relancer
Start-Sleep -Seconds 1
ViewClick -213 -12
ViewClick -23 -12
$c = Counts
Check "poutre créée au 2nd clic (2 nœuds, 1 élément)" ($c[0] -eq 2 -and $c[1] -eq 1) "$(CountsText) ; invite « $(PromptText 'Poutre B') »"
Shot "03a_poutre.png"
FocusMain; Press "{ESC}"; Press "{ESC}"; Start-Sleep -Milliseconds 500   # fin de l'enchaînement « Étirer »
Say "    bouton Annuler : $(UndoButtonInfo 'Annuler')"
Click-Mouse (TitleBarButton "Annuler"); Start-Sleep -Seconds 1
$c = Counts
Check "Annuler : poutre ET nœuds retirés" ($c[0] -eq 0 -and $c[1] -eq 0) (CountsText)
Click-Mouse (TitleBarButton "Rétablir"); Start-Sleep -Seconds 1
$c = Counts
Check "Rétablir : poutre et nœuds rendus" ($c[0] -eq 2 -and $c[1] -eq 1) (CountsText)

# --- 4. Dalle : 3 clics, Entrée -----------------------------------------------------------------------------------------
RibbonTab "Modèle"
Click-Mouse (RibbonButton "Dessiner*Dalle")
Start-Sleep -Seconds 2
$base = Base
ViewClick -203 98
ViewClick -53 138
ViewClick -143 188
$d = Delta $base
Check "dalle en cours (3 points) : aucun nœud ajouté" ($d[0] -eq 0 -and $d[1] -eq 0) (CountsText)
FocusMain; Press "{ENTER}"; Start-Sleep -Seconds 1
$d = Delta $base
Check "Entrée : dalle et ses 3 nœuds créés" ($d[0] -eq 3 -and $d[1] -eq 1) "$(CountsText) ; invite « $(PromptText 'Dalle S') »"
Shot "04_dalle.png"

# --- 5. Dalle croisée ----------------------------------------------------------------------------------------------------
$base = Base
ViewClick 17 28
ViewClick 137 78
ViewClick 137 28
ViewClick 17 78
FocusMain; Press "{ENTER}"; Start-Sleep -Seconds 1
$d = Delta $base
$msg = PromptText "Non créé"
Check "dalle croisée refusée, rien de créé" ($d[0] -eq 0 -and $d[1] -eq 0 -and $msg -match "recoupe") "$(CountsText) ; invite « $msg »"
Shot "05_dalle_croisee_refusee.png"
FocusMain; Press "{ESC}"

# --- 6. Voile à base inclinée (dialogue « Surface », onglet Voile) -------------------------------------------------------
$base = Base
RibbonTab "Modèle"
Click-Mouse (RibbonButton "Dessiner*Voile")
Start-Sleep -Seconds 2
$sd = ChildWindow "Éléments Surfaciques*" 10
if ($sd) {
    $edits = @(Find-All $sd $CT::Edit | Where-Object { -not $_.Current.IsOffscreen } | Sort-Object { $_.Current.BoundingRectangle.Top })
    $wallEdits = @($edits | Select-Object -Last 2)   # Origine puis Extrémité (bas du dialogue)
    Say "    champs du dialogue : $(($edits | ForEach-Object { '[' + (Value-Of $_) + ']' }) -join ' ')"
    Set-Value $wallEdits[0] "20 0 0"
    Set-Value $wallEdits[1] "25 0 0.4"
    Click-Mouse (Find-ByName (Fresh $sd) "Ajouter" $CT::Button)
    $mb = ChildWindow "Voile non créé" 3
    $warn = ""
    foreach ($try in 1..10) { $warn = PromptText "Base inclinée"; if ($warn) { break }; Start-Sleep -Milliseconds 500 }
    Shot "06_voile_incline.png"
    if ($mb) { Click (Find-ByName $mb "OK" $CT::Button) }
    Click (Find-ByName (Fresh $sd) "Fermer" $CT::Button)
    Start-Sleep -Seconds 1
    $script:wallBase = $base
    Check "voile incliné créé, avertissement « Base inclinée » (aucune boîte de refus)" ($mb -eq $null -and $warn -match "diffèrent de 0.400 m") "invite « $warn »"
} else { Check "dialogue Surface ouvert" $false "introuvable" }
FocusMain; Press "{ESC}"

# --- 7. Paramètres de modélisation : onglets Fondation et Treillis --------------------------------------------------------
RibbonTab "Modèle"
Click-Mouse (RibbonButton "Paramètres de*Modélisation*")
$pd = ChildWindow "*Pré-dessin 3D" 10
if ($pd) {
    $tabs = (Find-All $pd $CT::TabItem | ForEach-Object { $_.Current.Name }) -join " | "
    foreach ($t in @(Find-All $pd $CT::TabItem | Where-Object { $_.Current.Name -in "Fondation", "Treillis" })) {
        Click-Mouse $t; Start-Sleep -Milliseconds 600
        Capture (HandleOf $pd) (Join-Path $OutDir ("07_preglages_" + $t.Current.Name + ".png")) | Out-Null
    }
    Check "préréglages : onglets Fondation et Treillis" ($tabs -match "Fondation" -and $tabs -match "Treillis") $tabs
    Click (Find-ByName (Fresh $pd) "Annuler" $CT::Button)
} else { Check "dialogue des préréglages ouvert" $false "introuvable" }
Start-Sleep -Seconds 1

# --- 8. Portique : créé, puis refusé sur les mêmes pieds -------------------------------------------------------------------
RibbonTab "Modèle"
Click-Mouse (RibbonButton "Portique")
Start-Sleep -Seconds 1
$base = Base
if ($script:wallBase -and $base[0] -eq $script:wallBase[0] -and $base[1] -eq $script:wallBase[1]) {
    # Lecture valide antérieure au voile (illisible juste après son dialogue) : le voile ajoute 2 nœuds et 1 élément,
    # contrôlé ci-dessous par les compteurs combinés
    $base = @(($script:wallBase[0] + 2), ($script:wallBase[1] + 1))
    Say "    référence du portique : avant le voile + voile = $($base -join '/')"
}
ViewClick -123 -92
ViewClick -23 -82
Start-Sleep -Seconds 1
$d = Delta $base
Check "portique créé (4 nœuds, 3 éléments de plus)" ($d[0] -eq 4 -and $d[1] -eq 3) "$(CountsText) ; console « $(ConsoleText 'Portique cr') »"
$c = Counts
Check "voile incliné : 2 nœuds et 1 voile créés (compteurs avant le voile, puis après voile + portique)" ($script:wallBase -and $c[0] -eq $script:wallBase[0] + 2 + 4 -and $c[1] -eq $script:wallBase[1] + 1 + 3) "avant le voile $($script:wallBase -join '/') ; après le portique $($c -join '/')"
Shot "08a_portique.png"
$base = Base
ViewClick -123 -92
ViewClick -23 -82
Start-Sleep -Seconds 1
$d = Delta $base
$msg = ConsoleText "Portique non créé"
Check "portique relancé sur les mêmes pieds : refusé, rien de créé" ($d[0] -eq 0 -and $d[1] -eq 0 -and $msg -match "double") "$(CountsText) ; console « $msg »"
Shot "08b_portique_refuse.png"
FocusMain; Press "{ESC}"; Press "{ESC}"

# --- 9. Palette : identité BIM et quantités de la dalle (grande, loin des repères de niveau) --------------------------------
$props = Find-All (Fresh $script:w) $CT::TabItem | Where-Object { $_.Current.Name -like "PROPRI*" } | Select-Object -First 1
if ($props) { Click-Mouse $props; Start-Sleep -Milliseconds 600 }
ViewHoverClick -133 141   # centre de la dalle de l'étape 4
$bim = ""
foreach ($try in 1..16) { $bim = PromptText "Identité BIM|Classe IFC|GlobalId|Surface|Volume"; if ($bim -match "Classe IFC") { break }; Start-Sleep -Milliseconds 500 }
if (-not ($bim -match "Classe IFC")) {
    # Repli : sélection par l'arbre du modèle (catégorie « Dalles » dépliée, première dalle)
    $treeTab = Find-All (Fresh $script:w) $CT::TabItem | Where-Object { $_.Current.Name -like "ARBRE*" } | Select-Object -First 1
    if ($treeTab) { Click-Mouse $treeTab; Start-Sleep -Milliseconds 800 }
    $cat = Find-All (Fresh $script:w) $CT::TreeItem | Where-Object { $_.Current.Name -like "Dalles*" } | Select-Object -First 1
    if ($cat) {
        try { $cat.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern).Expand() } catch {}
        Start-Sleep -Milliseconds 800
        $item = @(Find-All $cat $CT::TreeItem | Where-Object { -not $_.Current.IsOffscreen })[0]
        if ($item) { Say "    sélection par l'arbre : « $($item.Current.Name) »"; Click-Mouse $item; Start-Sleep -Seconds 1 }
    } else { Say "    catégorie « Dalles » introuvable dans l'arbre" }
    $props = Find-All (Fresh $script:w) $CT::TabItem | Where-Object { $_.Current.Name -like "PROPRI*" } | Select-Object -First 1
    if ($props) { Click-Mouse $props; Start-Sleep -Milliseconds 800 }
    $bim = PromptText "Identité BIM|Classe IFC|GlobalId|Longueur|Volume"
}
Check "palette : classe IFC, GlobalId et quantités de la dalle" ($bim -match "IfcSlab" -and $bim -match "GlobalId" -and $bim -match "Surface" -and $bim -match "Volume") $(if ($bim) { $bim.Substring(0, [Math]::Min(400, $bim.Length)) } else { "rien (état : « $(PromptText 'sél\.') »)" })
foreach ($e in (Find-All (Fresh $script:w) $CT::Pane)) {
    # Zone défilante du panneau Propriétés (à gauche) : en bas, là où se trouve « Identité BIM et quantités »
    try {
        if ($e.Current.BoundingRectangle.Left -lt 360 -and $e.Current.BoundingRectangle.Top -gt 260) {
            $sp = $e.GetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern)
            if ($sp.Current.VerticallyScrollable) { $sp.SetScrollPercent(-1, 100); Start-Sleep -Milliseconds 500 }
        }
    } catch {}
}
Shot "09_palette_bim.png"

# --- 10. Arbre du modèle : branche « Structure BIM (IFC) » ------------------------------------------------------------------
RibbonTab "Affichage"
EnsureOn "ARBRE DU MOD*"   # panneau masqué dans certaines dispositions (dernière étape : plus de clic dans la vue)
Start-Sleep -Seconds 1
$treeTab = Find-All (Fresh $script:w) $CT::TabItem | Where-Object { $_.Current.Name -like "ARBRE*" } | Select-Object -First 1
if ($treeTab) { Click-Mouse $treeTab; Start-Sleep -Seconds 1 }
# UIA n'expose que les lignes visibles : filtre de l'arbre sur « Poteau C1 » (ancêtres gardés et dépliés)
# (un panneau replié garde des champs « visibles » pour UIA, à des coordonnées négatives : position réelle exigée)
$search = Find-All (Fresh $script:w) $CT::Edit | Where-Object { $r = $_.Current.BoundingRectangle; -not $_.Current.IsOffscreen -and $r.Left -ge 0 -and $r.Left -lt 480 -and $r.Top -ge 240 -and $r.Top -lt 330 } | Select-Object -First 1
if ($search) {
    Say "    champ de recherche : $($search.Current.BoundingRectangle) valeur « $(Value-Of $search) »"
    Click-Mouse $search; Press "^a"; Press "Poteau C1"; Start-Sleep -Seconds 1
    Say "    après saisie : « $(Value-Of $search) »"
} else { Say "    champ de recherche de l'arbre introuvable" }
$bimRoot = $null
foreach ($try in 1..10) {
    $bimRoot = Find-All (Fresh $script:w) $CT::TreeItem | Where-Object { $_.Current.Name -like "Structure BIM*" } | Select-Object -First 1
    if ($bimRoot) { break }; Start-Sleep -Milliseconds 500
}
# Qt expose les lignes d'un QTreeWidget à plat (pas d'imbrication UIA) : le filtre ne laisse visibles que le chemin
# du produit, lu dans l'ordre des lignes
$rows = @(Find-All (Fresh $script:w) $CT::TreeItem | Where-Object { $r = $_.Current.BoundingRectangle; -not $_.Current.IsOffscreen -and $r.Left -ge 0 -and $r.Left -lt 480 -and $r.Top -gt 330 } | Sort-Object { $_.Current.BoundingRectangle.Top })
$path = @($rows | ForEach-Object { $_.Current.Name })
$colItem = $rows | Where-Object { $_.Current.Name -like "Poteau C1*" } | Select-Object -First 1
if ($colItem) { Click-Mouse $colItem; Start-Sleep -Seconds 1 }
$propTitle = PromptText "PROPRIÉTÉS DU POTEAU"
$chain = $path -join " › "
Check "arbre : Structure BIM › Site › Bâtiment › étage › Poteau C1, clic → propriétés du poteau" ($bimRoot -and $colItem -and $chain -match "Structure BIM.*Site.*Bâtiment.*Poteau C1" -and $propTitle) "chemin « $chain » ; panneau « $propTitle »"
Shot "10_arbre_bim.png"

# --- 11. Types de produits : création depuis l'arbre (menu contextuel piloté au clavier : ses éléments ne sont pas
# exposés à UIA) -------------------------------------------------------------------------------------------------------
Guard
function TreeRows {
    @(Find-All (Fresh $script:w) $CT::TreeItem | Where-Object { $r = $_.Current.BoundingRectangle; -not $_.Current.IsOffscreen -and $r.Left -ge 0 -and $r.Left -lt 480 -and $r.Top -gt 330 } | Sort-Object { $_.Current.BoundingRectangle.Top })
}
function TreeFilter($text) {
    $f = Find-All (Fresh $script:w) $CT::Edit | Where-Object { $r = $_.Current.BoundingRectangle; -not $_.Current.IsOffscreen -and $r.Left -ge 0 -and $r.Left -lt 480 -and $r.Top -ge 240 -and $r.Top -lt 330 } | Select-Object -First 1
    if ($f) { Click-Mouse $f; Press "^a"; Press $text; Start-Sleep -Seconds 1 }
}
TreeFilter "Structure BIM"
$bimRow = TreeRows | Where-Object { $_.Current.Name -like "Structure BIM*" } | Select-Object -First 1
if ($bimRow) {
    $r = $bimRow.Current.BoundingRectangle
    [MouseInput]::SetCursorPos([int]($r.Left + 40), [int]($r.Top + $r.Height / 2)) | Out-Null; Start-Sleep -Milliseconds 300
    [MouseInput]::mouse_event(8, 0, 0, 0, [UIntPtr]::Zero); [MouseInput]::mouse_event(16, 0, 0, 0, [UIntPtr]::Zero)   # clic droit
    Start-Sleep -Milliseconds 900
    Press "{DOWN}"; Press "{ENTER}"; Start-Sleep -Seconds 1   # 1re entrée : « Créer les types de produits depuis le modèle »
}
$typesMsg = ConsoleText "Types de produits :"
TreeFilter "Types de produits"
$typeRows = @(TreeRows | ForEach-Object { $_.Current.Name })
Check "types créés depuis l'arbre (clic droit, menu), branche « Types de produits »" ($bimRow -and $typesMsg -match "type\(s\) créé" -and ($typeRows -join "|") -match "Types de produits") "console « $typesMsg » ; arbre « $($typeRows -join ' › ') »"
Shot "11_types_de_produits.png"

# Vue d'ensemble (dernière étape : le cadrage change) — voile incliné, dalle, portique, poutre visibles
RibbonTab "Affichage"
$fit = RibbonButton "Zoom Étendu*"
if ($fit) { Click-Mouse $fit; Start-Sleep -Seconds 2 }
Shot "11_vue_ensemble.png"

# --- Fermeture sans enregistrer, réglages restaurés ----------------------------------------------------------------------
Finish $(if (@($script:results | Where-Object { -not $_.Ok }).Count -eq 0) { 0 } else { 1 })
