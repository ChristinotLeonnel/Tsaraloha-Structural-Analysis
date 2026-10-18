# Vues 2D / 3D, ViewCube et plans de travail dans l'application réelle (UI Automation, vrais clics, vrais boutons du
# ruban) sur une COPIE d'un projet existant à structure 3D (examples/TSA_Demo_Batiment_R+5.tsa, ouvert en argument) :
#   1. projet ouvert, coordonnées de tous les nœuds relevées (tsa_nodes.py : lecture par TSA lui-même via tsa-ifc) ;
#   2. « Plan XY (Vue d'étage) » : mode 2D coché, bandeau du plan, vue annoncée « 2D — Plan (dessus) » ;
#   3. poutre tracée dans le plan (2 clics) : 2 nœuds, 1 élément ; Annuler puis Rétablir ;
#   4. « Plan XZ (Élévation) » : « 2D — Élévation avant (XZ) » ; poutre tracée dans le plan XZ ;
#   5. « Vue 3D » : mode 2D décoché ; ViewCube, face HAUT : « 3D — Vue de dessus » ; « Plan XY », puis coin du
#      ViewCube : retour en 3D (mode 2D décoché, « 3D — orientation libre ») ;
#   La vue annoncée est lue dans la console (« Vue : … », écrite à chaque changement par le service de navigation).
#   6. projet enregistré, TSA fermé : coordonnées réelles relues — les 380 nœuds d'origine inchangés, nœuds tracés
#      exactement dans leurs plans (Z = 0 pour le plan d'étage, Y = 0 pour l'élévation avant) ;
#   7. projet rouvert : nombre de nœuds et d'éléments retrouvé, « Plan XY » à nouveau en mode 2D.
# Garde-fous (comme drawing_scenario.ps1) : aucun clic dans la vue si la fenêtre n'est pas celle de la copie ; réglages
# HKCU\Software\TSA Engineering sauvegardés puis restaurés après la fermeture de TSA. Captures dans <Work>\shots,
# journal dans <Work>\views_scenario.log.
param([string]$Exe = "", [Parameter(Mandatory = $true)][string]$Work)
$Root = Join-Path $PSScriptRoot "..\.."
if (-not $Exe) { $Exe = Join-Path $Root "build-ninja-release\TSA.exe" }
$IfcTool = Join-Path (Split-Path $Exe) "tsa-ifc.exe"
$ErrorActionPreference = "Continue"
. (Join-Path $PSScriptRoot "uia_lib.ps1")
$OutDir = Join-Path $Work "shots"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$Log = Join-Path $Work "views_scenario.log"
"" | Set-Content $Log -Encoding UTF8
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
function RibbonButton($pattern) {
    foreach ($type in $CT::Button, $CT::CheckBox) {
        foreach ($b in (Find-All (Fresh $script:w) $type)) { if ($b.Current.Name -like $pattern -and -not $b.Current.IsOffscreen) { return $b } }
    }
    return $null
}
function TitleBarButton($name) {
    Find-All (Fresh $script:w) $CT::Button | Where-Object { $_.Current.Name -eq $name -and $_.Current.BoundingRectangle.Top -lt 60 } | Select-Object -First 1
}
function RibbonTab($name) {
    # Fenêtre ouverte par un fichier en argument : sans le focus, le clic souris sur l'onglet est ignoré ;
    # SelectionItemPattern sélectionne l'onglet dans tous les cas (vérifié, clic en dernier recours)
    foreach ($try in 1..3) {
        $t = Find-All (Fresh $script:w) $CT::TabItem | Where-Object { $_.Current.Name -eq $name } | Select-Object -First 1
        if (-not $t) { Start-Sleep -Milliseconds 500; continue }
        $sel = $null
        try { $sel = $t.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern) } catch {}
        if ($sel) { $sel.Select() } else { Click-Mouse $t }
        Start-Sleep -Milliseconds 800
        try { if ($sel -and $sel.Current.IsSelected) { return } } catch {}
    }
}
function PressRibbon($tab, $pattern) {
    Guard
    RibbonTab $tab
    $b = RibbonButton $pattern
    if (-not $b) { Say "ARRÊT : bouton « $pattern » introuvable (onglet $tab)"; Finish 2 }
    Click-Mouse $b; Start-Sleep -Seconds 2
}
function EnsureOff($pattern) {
    $b = RibbonButton $pattern
    if ($b) { try { if ((Toggle-State $b) -eq "On") { Click-Mouse $b } } catch {} ; Say "    $($b.Current.Name) : $(try { Toggle-State $b } catch { '?' })" }
}
function FocusMain { [Win]::SetForegroundWindow($script:w) | Out-Null; Start-Sleep -Milliseconds 300 }
function Counts {
    FocusMain
    foreach ($try in 1..25) {
        $t = @(Texts "^\d+ nœuds · \d+ élém") | Select-Object -First 1
        if ($t -match "^(\d+) nœuds · (\d+) élém") { $script:lastGood = @([int]$Matches[1], [int]$Matches[2]); return $script:lastGood }
        Start-Sleep -Milliseconds 400
    }
    return $script:lastGood
}
function Mode2D {
    $b = @(Find-All (Fresh $script:w) $CT::CheckBox | Where-Object { $_.Current.Name -eq "2D" })[0]
    if (-not $b) { return "?" }
    try { return [string](Toggle-State $b) } catch { return "?" }
}
function Badge { (Texts "2D \| ") -join " | " }
function Finish([int]$code) {
    $closed = Close-App $script:p $script:w 60 $false
    $script:p.Refresh()
    if (-not $script:p.HasExited -and $script:projTitle -and ($script:p.MainWindowTitle -replace ' \*$', '') -eq $script:projTitle) {
        Say "    fermeture forcée de la copie d'essai"; Stop-Process -Id $script:p.Id -Force; $closed = $true
    }
    try { $script:p.WaitForExit(15000) | Out-Null } catch {}
    $ok = @($script:results | Where-Object { $_.Ok }).Count
    Say "fin : application fermée = $closed ; $ok / $($script:results.Count) vérifications réussies"
    if ($script:p.HasExited -and (Test-Path $RegBackup)) { & reg.exe delete "HKCU\Software\TSA Engineering" /f *> $null; & reg.exe import $RegBackup *> $null }
    elseif (-not $script:p.HasExited) { Say "    ATTENTION : TSA encore ouvert, réglages NON restaurés (sauvegarde : $RegBackup)" }
    exit $code
}
function Guard {
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
function ViewPoint($fx, $fy) {
    # Point de la vue en fraction de sa taille (0,5 ; 0,5 = centre)
    $r = ViewRect
    if (-not $r) { Say "ARRÊT : vue 3D (OccView) introuvable"; Finish 2 }
    return @([int]($r.Left + $r.Width * $fx), [int]($r.Top + $r.Height * $fy))
}
function ViewClick($fx, $fy) { Guard; $pt = ViewPoint $fx $fy; [MouseInput]::Click($pt[0], $pt[1]); Start-Sleep -Milliseconds 800 }
function EnsureOn($pattern) {
    $b = RibbonButton $pattern
    if ($b) { try { if ((Toggle-State $b) -ne "On") { Click-Mouse $b } } catch {} ; Say "    $($b.Current.Name) : $(try { Toggle-State $b } catch { '?' })" }
}
function LastView {
    # Dernière vue annoncée dans la console (QTextEdit lu par TextPattern) : « [hh:mm:ss] [INFO] … Vue : 2D — … »
    FocusMain
    foreach ($try in 1..10) {
        foreach ($type in $CT::Document, $CT::Edit) {
            foreach ($e in (Find-All (Fresh $script:w) $type)) {
                try {
                    $txt = $e.GetCurrentPattern([System.Windows.Automation.TextPattern]::Pattern).DocumentRange.GetText(-1)
                    $lines = @(($txt -split "`n") | Where-Object { $_ -match "Vue : " })
                    if ($lines.Count) { return ($lines[-1] -replace '^.*Vue : ', '').Trim() }
                } catch {}
            }
        }
        Start-Sleep -Milliseconds 400
    }
    return ""
}
function CloseToolDialog {
    # Fenêtre de l'outil de dessin (« Éléments Filaires… ») : posée sur la vue, elle masquerait le ViewCube
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $script:p.Id)
    $winCond = New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $CT::Window)
    foreach ($top in $AE::RootElement.FindAll($TS::Children, $cond)) {
        foreach ($win in @($top) + @($top.FindAll($TS::Descendants, $winCond))) {
            if ($win.Current.Name -like "Éléments Filaires*") {
                $b = Find-ByName $win "Fermer" $CT::Button
                if ($b) { Click $b; Start-Sleep -Milliseconds 800; Say "    fenêtre « $($win.Current.Name) » fermée"; return }
            }
        }
    }
}
function CubeClick($dx, $dy) {
    # ViewCube : coin supérieur droit de la vue, centre à 85 px des bords (Graphic3d_TransformPers de OccView)
    Guard
    $r = ViewRect
    $x = [int]($r.Right - 85 + $dx); $y = [int]($r.Top + 85 + $dy)
    [MouseInput]::SetCursorPos($x - 3, $y) | Out-Null; Start-Sleep -Milliseconds 250
    [MouseInput]::SetCursorPos($x, $y) | Out-Null; Start-Sleep -Milliseconds 500
    [MouseInput]::Click($x, $y); Start-Sleep -Milliseconds 1200
}
function ReadNodes($file, $json) {
    & python (Join-Path $PSScriptRoot "tsa_nodes.py") $IfcTool $file $json | Out-Null
    if (-not (Test-Path $json)) { return $null }
    return (Get-Content $json -Raw -Encoding UTF8 | ConvertFrom-Json)
}
function Launch($file) {
    $script:p = Start-Process -FilePath $Exe -ArgumentList "`"$file`"" -PassThru
    $deadline = (Get-Date).AddSeconds(120)
    $expected = "TSA — $(Split-Path $file -Leaf)"
    while ((Get-Date) -lt $deadline) {
        $script:p.Refresh()
        if ($script:p.MainWindowHandle -ne [IntPtr]::Zero -and ($script:p.MainWindowTitle -replace ' \*$', '') -eq $expected) { break }
        Start-Sleep -Milliseconds 500
    }
    $script:w = $script:p.MainWindowHandle
    Start-Sleep -Seconds 6
    $wa = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
    [Win]::ShowWindow($script:w, 9) | Out-Null
    [Win]::MoveWindow($script:w, $wa.Left, $wa.Top, $wa.Width, $wa.Height, $true) | Out-Null
    Start-Sleep -Seconds 2
    $script:p.Refresh()
    return (($script:p.MainWindowTitle -replace ' \*$', '') -eq $expected)
}

# --- 1. Copie du projet, coordonnées d'origine, ouverture -------------------------------------------------------------
$source = Join-Path $Root "examples\TSA_Demo_Batiment_R+5.tsa"
$copy = Join-Path $Work "Vues_R+5.tsa"
Copy-Item $source $copy -Force
$before = ReadNodes $copy (Join-Path $Work "nodes_before.json")
$nBefore = @($before.PSObject.Properties).Count
Say "1. copie du projet de démonstration : $copy ($nBefore nœuds relus)"
if (-not (Launch $copy)) { Say "ARRÊT : la copie d'essai n'est pas la fenêtre active (« $($script:p.MainWindowTitle) »)"; $script:projTitle = $null; Finish 2 }
$script:projTitle = "TSA — Vues_R+5.tsa"
$c0 = Counts
Check "copie du projet ouverte (structure 3D)" ($nBefore -eq 380 -and $c0[0] -eq 380) "$($c0[0]) nœud(s), $($c0[1]) élément(s) ; $nBefore nœuds dans le fichier"
RibbonTab "Affichage"
EnsureOff "Magnétisme Grille*"
EnsureOff "Accrochage Objets*"
EnsureOn "CONSOLE*DIAGNOSTICS"
Shot "01_projet_3D.png"

# --- 2. Plan XY : vrai plan 2D ------------------------------------------------------------------------------------------
PressRibbon "Affichage" "Plan*XY*(Vue*"
$m2 = Mode2D
$v2 = LastView
Check "« Plan XY » : mode 2D, bandeau du plan, vue « 2D — Plan (dessus) »" ($m2 -eq "On" -and (Badge) -match "2D" -and $v2 -eq "2D — Plan (dessus)") "case 2D = $m2 ; bandeau « $(Badge) » ; vue « $v2 »"
Shot "02_plan_XY.png"

# --- 3. Poutre dans le plan, Annuler, Rétablir ------------------------------------------------------------------------
$b3 = Counts
PressRibbon "Modèle" "Dessiner*Poutre"
ViewClick 0.30 0.30
ViewClick 0.42 0.30
FocusMain; Press "{ESC}"; Press "{ESC}"; Start-Sleep -Milliseconds 600
CloseToolDialog
$c3 = Counts
Check "plan XY : poutre tracée (2 nœuds, 1 élément de plus)" (($c3[0] - $b3[0]) -eq 2 -and ($c3[1] - $b3[1]) -eq 1) "avant $($b3 -join '/') ; après $($c3 -join '/')"
$undo = TitleBarButton "Annuler"; if ($undo) { Click-Mouse $undo; Start-Sleep -Seconds 1 }
$cu = Counts
$redo = TitleBarButton "Rétablir"; if ($redo) { Click-Mouse $redo; Start-Sleep -Seconds 1 }
$cr = Counts
Check "Annuler puis Rétablir après la poutre du plan" ($cu[0] -eq $b3[0] -and $cu[1] -eq $b3[1] -and $cr[0] -eq $c3[0] -and $cr[1] -eq $c3[1]) "après Annuler $($cu -join '/') ; après Rétablir $($cr -join '/')"
Shot "03_poutre_plan.png"

# --- 4. Élévation avant (plan XZ) ---------------------------------------------------------------------------------------
PressRibbon "Affichage" "Plan*XZ*(*"
$v4 = LastView
Check "« Plan XZ » : élévation 2D, vue « 2D — Élévation avant (XZ) »" ((Mode2D) -eq "On" -and $v4 -eq "2D — Élévation avant (XZ)") "case 2D = $(Mode2D) ; bandeau « $(Badge) » ; vue « $v4 »"
$b4 = Counts
PressRibbon "Modèle" "Dessiner*Poutre"
ViewClick 0.25 0.35
ViewClick 0.25 0.55
FocusMain; Press "{ESC}"; Press "{ESC}"; Start-Sleep -Milliseconds 600
CloseToolDialog
$c4 = Counts
Check "plan XZ : poutre tracée (2 nœuds, 1 élément de plus)" (($c4[0] - $b4[0]) -eq 2 -and ($c4[1] - $b4[1]) -eq 1) "avant $($b4 -join '/') ; après $($c4 -join '/')"
Shot "04_elevation_XZ.png"

# --- 5. Retour en 3D, ViewCube ------------------------------------------------------------------------------------------
PressRibbon "Affichage" "Vue*3D*(Axono*"
$v5 = LastView
Check "« Vue 3D » : mode 2D décoché, vue 3D annoncée" ((Mode2D) -eq "Off" -and $v5 -like "3D — *") "case 2D = $(Mode2D) ; vue « $v5 »"
Shot "05_retour_3D.png"
CubeClick 0 -25   # face HAUT, vue isométrique (centre du cube = coin avant-droit-haut)
$v6 = LastView
Check "ViewCube, face HAUT : vue de dessus, en 3D" ($v6 -eq "3D — Vue de dessus" -and (Mode2D) -eq "Off") "vue « $v6 » ; case 2D = $(Mode2D)"
Shot "06_cube_haut.png"
PressRibbon "Affichage" "Plan*XY*(Vue*"
CubeClick 37 37   # coin arrondi du cube vu de dessus (face HAUT ≈ ±31 px, bordure des arêtes ≈ 8 px)
$v7 = LastView
Check "mode 2D puis coin du ViewCube : retour en 3D" ((Mode2D) -eq "Off" -and $v7 -eq "3D — orientation libre") "case 2D = $(Mode2D) ; vue « $v7 »"
Shot "07_cube_coin.png"

# --- 6. Enregistrement, coordonnées réelles ----------------------------------------------------------------------------
$cBeforeSave = Counts
FocusMain; Press "^s"; Start-Sleep -Seconds 3
$closed = Close-App $script:p $script:w 60 $false
$script:p.Refresh()
Say "6. projet enregistré puis fermé (fermé = $closed)"
$after = ReadNodes $copy (Join-Path $Work "nodes_after.json")
$unchanged = 0; $moved = @()
foreach ($prop in $before.PSObject.Properties) {
    $a = $after.($prop.Name)
    if ($a -and $a[0] -eq $prop.Value[0] -and $a[1] -eq $prop.Value[1] -and $a[2] -eq $prop.Value[2]) { $unchanged++ } else { $moved += $prop.Name }
}
$new = @($after.PSObject.Properties | Where-Object { -not $before.($_.Name) })
Say "    nœuds nouveaux : $(($new | ForEach-Object { "$($_.Name)=($($_.Value -join ', '))" }) -join ' ; ')"
Check "les $nBefore nœuds d'origine ont exactement leurs coordonnées" ($unchanged -eq $nBefore) "$unchanged inchangés ; modifiés : $($moved -join ', ')"
$inXY = @($new | Where-Object { $_.Value[2] -eq 0.0 -and $_.Value[1] -ne 0.0 }).Count
$inXZ = @($new | Where-Object { $_.Value[1] -eq 0.0 -and $_.Value[2] -ne 0.0 }).Count
Check "nœuds tracés exactement dans leurs plans (2 à Z = 0, 2 à Y = 0)" ($new.Count -eq 4 -and $inXY -eq 2 -and $inXZ -eq 2) "$($new.Count) nouveaux ; plan d'étage (Z = 0) : $inXY ; élévation (Y = 0) : $inXZ"

# --- 7. Réouverture ----------------------------------------------------------------------------------------------------
if (-not (Launch $copy)) { Say "ARRÊT : réouverture de la copie impossible"; Finish 2 }
$c7 = Counts
PressRibbon "Affichage" "Plan*XY*(Vue*"
Check "projet rouvert : mêmes nombres, « Plan XY » à nouveau en mode 2D" ($c7[0] -eq $cBeforeSave[0] -and $c7[1] -eq $cBeforeSave[1] -and (Mode2D) -eq "On") "$($c7 -join '/') (avant enregistrement $($cBeforeSave -join '/')) ; case 2D = $(Mode2D)"
Shot "08_reouvert_plan_XY.png"
PressRibbon "Affichage" "Vue*3D*(Axono*"
Finish 0
