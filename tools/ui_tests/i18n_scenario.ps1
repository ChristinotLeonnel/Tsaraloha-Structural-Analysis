# Langues, ruban et recherche globale dans l'application réelle (UI Automation) sur une COPIE de
# examples/TSA_Demo_Batiment_R+5.tsa (jamais enregistrée) — docs/I18N.md, docs/UI_RIBBON.md, docs/UI_SEARCH.md :
#   pour chaque langue (fr, en, mg, imposée par --lang=) :
#     1. dix onglets du ruban dans la langue, capture de chacun (<Work>\shots\<langue>_<n>.png) ;
#     2. chaque bouton du ruban a un nom accessible (boutons repérés par l'identifiant de leur commande, AutomationId) ;
#     3. recherche (Ctrl+F) « afficher la grille » / « show grid » / « asehoy ny makarakara » : premier résultat dans la
#        langue, Entrée → la vraie commande s'exécute (bouton cmd.display.grid basculé, lu par son identifiant) ;
#     4. recherche de la langue → boîte « Langue de l'interface » : trois langues nommées dans leur langue ;
#        (anglais seulement) malgache choisi « au prochain démarrage » → réglage ui/language = mg ;
#   puis :
#     5. relance SANS --lang : la préférence enregistrée (malgache) est appliquée ;
#     6. boîte de langue : français + « Redémarrer maintenant » → TSA se ferme, se relance seul en français et rouvre la
#        copie du projet.
# Garde-fous : réglages HKCU\Software\TSA Engineering sauvegardés puis restaurés après l'arrêt de TSA ; seuls les
# processus lancés par l'essai sont arrêtés ; aucun enregistrement du projet.
param([string]$Exe = "", [Parameter(Mandatory = $true)][string]$Work)
$Root = Join-Path $PSScriptRoot "..\.."
if (-not $Exe) { $Exe = Join-Path $Root "build-ninja-release\TSA.exe" }
$ErrorActionPreference = "Continue"
. (Join-Path $PSScriptRoot "uia_lib.ps1")
$OutDir = Join-Path $Work "shots"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$Log = Join-Path $Work "i18n_scenario.log"
"" | Set-Content $Log -Encoding UTF8
function Say($text) { Write-Host $text; Add-Content $Log $text -Encoding UTF8 }
$script:results = @()
function Check($label, [bool]$ok, $observed) {
    $script:results += [pscustomobject]@{ Step = $label; Ok = $ok }
    Say ("    [{0}] {1} — observé : {2}" -f $(if ($ok) { "OK" } else { "ÉCHEC" }), $label, $observed)
}

$RegBackup = Join-Path $Work "tsa_settings.reg"
& reg.exe export "HKCU\Software\TSA Engineering" $RegBackup /y *> $null
$Copy = Join-Path $Work "Langues_R+5.tsa"
Copy-Item (Join-Path $Root "examples\TSA_Demo_Batiment_R+5.tsa") $Copy -Force
$script:started = @()

$Expected = @{
    fr = @{ Tabs = @("Accueil", "Modélisation", "Modifier", "Grille et plans", "Charges", "Maillage", "Analyse", "Résultats", "Affichage", "Outils");
            Grid = "afficher la grille"; GridTitle = "Afficher la grille"; Lang = "langue"; Dialog = "Langue de l'interface" }
    en = @{ Tabs = @("Home", "Modelling", "Modify", "Grid and planes", "Loads", "Mesh", "Analysis", "Results", "View", "Tools");
            Grid = "show grid"; GridTitle = "Show grid"; Lang = "language"; Dialog = "Interface language" }
    mg = @{ Tabs = @("Fandraisana", "Modely", "Fanovana", "Makarakara sy planina", "Enta", "Harato", "Kajy", "Vokatra", "Fampisehoana", "Fitaovana");
            Grid = "asehoy ny makarakara"; GridTitle = "Asehoy ny makarakara"; Lang = "fiteny"; Dialog = "Fitenin'ny fifandraisana" }
}

function Launch([string[]]$arguments) {
    $p = Start-Process -FilePath $Exe -ArgumentList $arguments -PassThru
    $script:started += $p
    $w = Find-Dialog $p.Id "TSA*Langues_R+5*" 90
    # Fenêtre agrandie : groupes du ruban dépliés (boutons visibles par UI Automation)
    if ($w) { [Win]::ShowWindow((HandleOf $w), 3) | Out-Null; Start-Sleep -Seconds 4; Focus-Window $w }
    return @($p, $w)
}
function Park { $s = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds; [MouseInput]::SetCursorPos($s.Width - 2, $s.Height - 2) | Out-Null; Start-Sleep -Milliseconds 300 }
function Stop($p) { try { if (-not $p.HasExited) { $p.Kill(); $p.WaitForExit(15000) | Out-Null } } catch {} }
function RibbonTabs($w) {
    # Onglets du ruban : ceux qui suivent l'onglet du document (nom du fichier) et précèdent les panneaux ancrés
    return @(Find-All (Fresh $w) $CT::TabItem | ForEach-Object { $_.Current.Name } | Where-Object { $_ -notmatch "\.tsa$" })
}
function SelectTab($w, $name) {
    $t = Find-All (Fresh $w) $CT::TabItem | Where-Object { $_.Current.Name -eq $name } | Select-Object -First 1
    if (-not $t) { return $false }
    try { $t.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Select() } catch { Click-Mouse $t }
    Start-Sleep -Milliseconds 700
    return $true
}
function ById($w, $id) {
    # Bouton cochable : exposé comme case à cocher par Qt
    foreach ($ct in @($CT::Button, $CT::CheckBox)) { foreach ($e in (Find-All (Fresh $w) $ct)) { if ($e.Current.AutomationId -like "*$id") { return $e } } }
    return $null
}
function SearchFor($w, $text) {
    Focus-Window $w
    Press "^f"; Start-Sleep -Milliseconds 400
    Press "^a"; Press $text; Start-Sleep -Milliseconds 900
    return @(Find-All (Fresh $w) $CT::ListItem | ForEach-Object { $_.Current.Name })
}
function LanguageDialog($p, $title) { return Find-Dialog $p.Id $title 10 }

Say "Essai langues / ruban / recherche — $(Get-Date -Format 'yyyy-MM-dd HH:mm')"
foreach ($lang in @("fr", "en", "mg")) {
    $e = $Expected[$lang]
    Say "== Langue $lang"
    $p, $w = Launch @("--lang=$lang", ('"' + $Copy + '"'))
    if (-not $w) { Check "$lang : fenêtre de la copie ouverte" $false "introuvable"; Stop $p; continue }

    # 1. Onglets du ruban et captures
    $tabs = RibbonTabs $w
    $ribbon = @($tabs | Where-Object { $e.Tabs -contains $_ })
    Check "$lang : dix onglets du ruban dans la langue" (($ribbon -join "|") -eq ($e.Tabs -join "|")) ($ribbon -join " | ")
    $unnamed = 0; $buttons = 0
    for ($i = 0; $i -lt $e.Tabs.Count; $i++) {
        if (SelectTab $w $e.Tabs[$i]) {
            Park; Capture $w (Join-Path $OutDir ("{0}_{1:00}_{2}.png" -f $lang, ($i + 1), ($e.Tabs[$i] -replace '[^\w]', '_'))) | Out-Null
            foreach ($b in (Find-All (Fresh $w) $CT::Button)) {
                if ($b.Current.AutomationId -match "cmd\.") { $buttons++; if (-not $b.Current.Name.Trim()) { $unnamed++ } }
            }
        }
    }
    Check "$lang : boutons du ruban nommés (lecteur d'écran)" ($buttons -gt 100 -and $unnamed -eq 0) "$buttons bouton(s) visités, $unnamed sans nom"

    # 3. Recherche → vraie commande
    SelectTab $w $e.Tabs[3] | Out-Null
    $grid = ById $w "cmd.display.grid"
    $before = if ($grid) { Toggle-State $grid } else { "?" }
    $items = SearchFor $w $e.Grid
    $first = ($items | Select-Object -First 1)
    Check "$lang : « $($e.Grid) » → premier résultat « $($e.GridTitle) »" ($first -like "$($e.GridTitle)*") "« $first »"
    Press "{ENTER}"; Start-Sleep -Milliseconds 1200
    $grid = ById $w "cmd.display.grid"
    $after = if ($grid) { Toggle-State $grid } else { "?" }
    Check "$lang : Entrée exécute la commande réelle (grille basculée, bouton lu par son identifiant)" ($before -ne "?" -and $after -ne $before) "$before → $after"

    # 4. Boîte de langue
    $items = SearchFor $w $e.Lang
    Press "{ENTER}"; Start-Sleep -Milliseconds 1200
    $dlg = LanguageDialog $p $e.Dialog
    if ($dlg) {
        $langs = @(Find-All $dlg $CT::ListItem | ForEach-Object { $_.Current.Name.Trim() })
        Check "$lang : boîte « $($e.Dialog) » : langues nommées dans leur langue" (($langs -join "|") -match "English.*Français.*Malagasy") ($langs -join " | ")
        Capture $dlg (Join-Path $OutDir "$lang`_langue.png") | Out-Null
        if ($lang -eq "en") {
            $mg = Find-All $dlg $CT::ListItem | Where-Object { $_.Current.Name -like "Malagasy*" } | Select-Object -First 1
            $mg.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Select(); Start-Sleep -Milliseconds 600
            $dlg = LanguageDialog $p "Fitenin'ny fifandraisana"
            $later = $null; foreach ($b in (Find-All $dlg $CT::Button)) { if ($b.Current.AutomationId -like "*TSALanguageLater") { $later = $b } }
            Check "en : explications de la boîte affichées dans la langue choisie (malgache)" ($null -ne $dlg -and $null -ne $later) "titre « $(if ($dlg) { $dlg.Current.Name }) », bouton « $(if ($later) { $later.Current.Name }) »"
            if ($later) { Click-Mouse $later; Start-Sleep -Seconds 1 }
            $stored = (& reg.exe query "HKCU\Software\TSA Engineering\TSA\ui" /v language 2>$null) -join " "
            Check "en : « au prochain démarrage » enregistre la préférence (ui/language = mg)" ($stored -match "language\s+REG_SZ\s+mg") $stored.Trim()
        } else { Press "{ESC}" }
    } else { Check "$lang : boîte de langue ouverte par la recherche" $false "introuvable" }
    Stop $p
}

# 5. Préférence appliquée au lancement suivant (sans --lang)
Say "== Relance sans --lang (préférence enregistrée)"
$p, $w = Launch @(('"' + $Copy + '"'))
if ($w) {
    $tabs = RibbonTabs $w
    Check "préférence relue au démarrage : ruban en malgache" (($tabs -join "|") -match "Fandraisana.*Makarakara sy planina.*Fitaovana") ($tabs -join " | ")
    # 6. Redémarrage immédiat vers le français
    $items = SearchFor $w "fiteny"
    Press "{ENTER}"; Start-Sleep -Milliseconds 1200
    $dlg = LanguageDialog $p "Fitenin'ny fifandraisana"
    if ($dlg) {
        $fr = Find-All $dlg $CT::ListItem | Where-Object { $_.Current.Name -like "Fran*" } | Select-Object -First 1
        $fr.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Select(); Start-Sleep -Milliseconds 600
        $dlg = LanguageDialog $p "Langue de l'interface"
        $restart = $null; foreach ($b in (Find-All $dlg $CT::Button)) { if ($b.Current.AutomationId -like "*TSALanguageRestart") { $restart = $b } }
        if ($restart) { Click-Mouse $restart }
        $exited = $p.WaitForExit(30000)
        $deadline = (Get-Date).AddSeconds(90); $w2 = $null; $p2 = $null
        while (-not $w2 -and (Get-Date) -lt $deadline) {
            foreach ($cand in (Get-Process TSA -ErrorAction SilentlyContinue | Where-Object { $_.Id -ne $p.Id -and $_.StartTime -gt (Get-Date).AddMinutes(-2) })) {
                $w2 = Find-Dialog $cand.Id "TSA*Langues_R+5*" 2
                if ($w2) { $p2 = $cand; $script:started += $cand; break }
            }
            Start-Sleep -Milliseconds 500
        }
        Check "« Redémarrer maintenant » : TSA fermé puis relancé, copie du projet rouverte" ($exited -and $null -ne $w2) "fermé : $exited ; nouvelle fenêtre : $(if ($w2) { $w2.Current.Name } else { 'aucune' })"
        if ($w2) {
            Start-Sleep -Seconds 4
            $tabs = RibbonTabs $w2
            Check "après redémarrage : ruban en français" (($tabs -join "|") -match "Accueil.*Grille et plans.*Outils") ($tabs -join " | ")
            Stop $p2
        }
    } else { Check "boîte de langue (malgache) ouverte" $false "introuvable" }
}
foreach ($sp in $script:started) { Stop $sp }

Start-Sleep -Seconds 2
& reg.exe import $RegBackup *> $null
$ok = @($script:results | Where-Object { $_.Ok }).Count
Say ("Bilan : {0} / {1} vérifications réussies ; réglages restaurés ; captures : {2}" -f $ok, $script:results.Count, $OutDir)
exit $(if ($ok -eq $script:results.Count) { 0 } else { 1 })
