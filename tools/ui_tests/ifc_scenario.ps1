# Échange IFC dans l'application réelle (UI Automation, vrais clics et vrais dialogues) :
#   1. projet existant ouvert ;
#   2-4. import IFC2X3, IFC4, IFC4X3_ADD2 : fenêtre d'options, rapport, nouvel onglet, projet d'origine intact ;
#   5. annulation pendant l'import d'un grand fichier : aucun onglet créé, projet intact ;
#   6. fichier tronqué : message d'erreur, aucun onglet créé ;
#   7. vue 3D : objets importés tracés, bouton « Objets IFC importés » (masquer / réafficher) ;
#   8. survol et clic sur un objet : propriétés IFC dans l'inspecteur (étage, matériaux, parent…) ;
#   9. export IFC : message de fin, fichier relu ensuite par tsa-ifc (IfcOpenShell, lecteur indépendant de l'écrivain).
# Captures dans <Work>\shots, journal dans <Work>\ifc_scenario.log. Les réglages HKCU\Software\TSA Engineering sont
# sauvegardés avant l'essai et restaurés après.
param([string]$Exe = "", [Parameter(Mandatory = $true)][string]$Work, [string]$BigFile = "")
if (-not $Exe) { $Exe = Join-Path $PSScriptRoot "..\..\build-ninja-release\TSA.exe" }
$ErrorActionPreference = "Continue"
. (Join-Path $PSScriptRoot "uia_lib.ps1")
$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Data = Join-Path $Root "tests\data\ifc\buildingsmart"
$OutDir = Join-Path $Work "shots"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$Log = Join-Path $Work "ifc_scenario.log"
"" | Set-Content $Log -Encoding UTF8
# Write-Host et non Write-Output : dans une fonction, la sortie du pipeline deviendrait sa valeur de retour
function Say($text) { Write-Host $text; Add-Content $Log $text -Encoding UTF8 }

# --- Réglages de TSA sauvegardés -----------------------------------------------------------------------------------
$RegBackup = Join-Path $Work "tsa_settings.reg"
& reg.exe export "HKCU\Software\TSA Engineering" $RegBackup /y *> $null
$Project = Join-Path $Work "Projet_ouvert.tsa"
Copy-Item (Join-Path $Root "examples\TSA_Exemple_Dalle_Coques.tsa") $Project -Force
$Truncated = Join-Path $Work "tronque.ifc"
$bytes = [IO.File]::ReadAllBytes((Join-Path $Data "ifc4_Building-Structural.ifc"))
[IO.File]::WriteAllBytes($Truncated, $bytes[0..([int]($bytes.Length * 0.4))])
$Exported = Join-Path $Work "export_tsa.ifc"
Remove-Item $Exported -ErrorAction SilentlyContinue

function Park { $s = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds; [MouseInput]::SetCursorPos($s.Width - 2, $s.Height - 2) | Out-Null; Start-Sleep -Milliseconds 400 }
function Shot($name) { Park; Capture $script:w (Join-Path $OutDir $name) | Out-Null; Say "    capture : shots\$name" }
function Tabs { @(Find-All (Fresh $script:w) $CT::TabItem | Where-Object { $_.Current.Name -match "\.tsa|Projet|Building|Nouveau|\(IFC\)|_" }) }
function TabNames { (Tabs | ForEach-Object { $_.Current.Name }) -join " | " }
function Texts($pattern, $root = $null) {
    if (-not $root) { $root = Fresh $script:w }
    $out = @(); foreach ($t in (Find-All $root $CT::Text)) { $n = $t.Current.Name; if ($n -match $pattern) { $out += $n } }
    return ($out | Select-Object -Unique)
}
function RibbonButton($pattern) {
    # Bouton cochable : exposé par Qt comme case à cocher
    foreach ($type in $CT::Button, $CT::CheckBox) {
        foreach ($b in (Find-All (Fresh $script:w) $type)) { if ($b.Current.Name -like $pattern -and -not $b.Current.IsOffscreen) { return $b } }
    }
    return $null
}
function ProcessWindows {
    # Fenêtres de premier niveau du processus (la condition ProcessId de UIA omet les boîtes natives #32770 : filtre
    # fait ici), et fenêtres Qt filles
    $winCond = New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $CT::Window)
    $all = @()
    foreach ($top in $AE::RootElement.FindAll($TS::Children, [System.Windows.Automation.Condition]::TrueCondition)) {
        try { if ($top.Current.ProcessId -ne $script:p.Id) { continue } } catch { continue }
        $all += $top
        $all += @($top.FindAll($TS::Descendants, $winCond))
    }
    return $all
}
function WaitWindow($test, $seconds) {
    $deadline = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $deadline) {
        foreach ($win in (ProcessWindows)) { try { if (& $test $win) { return $win } } catch {} }
        Start-Sleep -Milliseconds 300
    }
    return $null
}
function IsNative($win) { try { return $win.Current.ClassName -eq "#32770" } catch { return $false } }
function FillFileDialog($title, $path) {
    # Boîte native (#32770) : champ « Nom du fichier » non exposé à UIA dans cet état ; Alt+N y place le focus, chemin
    # saisi au clavier (chemin long : « ~ » serait une touche) puis Entrée.
    $dlg = WaitWindow { param($x) $x.Current.Name -like $title -and (IsNative $x) } 20
    if (-not $dlg) { Say "    boîte « $title » introuvable"; return $false }
    Focus-Window $dlg
    $escaped = [regex]::Replace($path, '[+^%~(){}\[\]]', { param($m) '{' + $m.Value + '}' })
    Press "%n"; Press "^a"; Press $escaped; Start-Sleep -Milliseconds 300; Press "{ENTER}"
    Start-Sleep -Seconds 1
    # Export : « remplacer le fichier ? » éventuel
    $confirm = WaitWindow { param($x) (IsNative $x) -and (Find-ByName $x "Oui" $CT::Button) } 2
    if ($confirm) { ClickButtonIn $confirm "Oui" | Out-Null }
    return $true
}
function ClickButtonIn($win, $name) {
    foreach ($b in (Find-All $win $CT::Button)) { if ($b.Current.Name -like $name) { Click-Mouse $b; return $true } }
    return $false
}
function MessageText($win) { (Find-All $win $CT::Text | ForEach-Object { $_.Current.Name } | Where-Object { $_ }) -join " / " }

function ImportIfc($file, [switch]$Cancel) {
    $btn = RibbonButton "Importer I*FC*"
    if (-not $btn) { Say "    bouton « Importer IFC » introuvable"; return $null }
    Click-Mouse $btn
    if (-not (FillFileDialog "Importer un fichier IFC" $file)) { return $null }
    $opts = WaitWindow { param($x) $x.Current.Name -like "Importer un fichier IFC" -and -not (IsNative $x) -and (Find-ByName $x "Importer" $CT::Button) } 20
    if (-not $opts) { Say "    fenêtre d'options introuvable"; return $null }
    $checks = (Find-All $opts $CT::CheckBox | ForEach-Object { $_.Current.Name.Substring(0, [Math]::Min(45, $_.Current.Name.Length)) + " = " + (Toggle-State $_) }) -join " ; "
    Say "    options : $checks"
    Capture (HandleOf $opts) (Join-Path $OutDir ("options_" + [IO.Path]::GetFileNameWithoutExtension($file) + ".png")) | Out-Null
    ClickButtonIn $opts "Importer" | Out-Null
    if ($Cancel) {
        $prog = WaitWindow { param($x) $x.Current.Name -eq "Import IFC" -and (Find-ByName $x "Annuler" $CT::Button) } 15
        if ($prog) {
            Start-Sleep -Milliseconds 700
            Capture (HandleOf $prog) (Join-Path $OutDir "progression_import.png") | Out-Null
            Say "    progression : « $(MessageText $prog) » → Annuler"
            ClickButtonIn $prog "Annuler" | Out-Null
            Start-Sleep -Milliseconds 300
            Say "    après Annuler : « $(MessageText $prog) »"
        } else { Say "    fenêtre de progression introuvable (import trop rapide ?)" }
    }
    # Fin : rapport (succès), message d'erreur, ou rien (annulation)
    $end = WaitWindow { param($x) $x.Current.Name -eq "Rapport d'import IFC" -or ($x.Current.Name -eq "Import IFC" -and -not (Find-ByName $x "Annuler" $CT::Button)) } 90
    return $end
}

# --- 1. Projet existant ---------------------------------------------------------------------------------------------
$script:p = Start-Process -FilePath $Exe -ArgumentList ('"' + $Project + '"') -PassThru
$deadline = (Get-Date).AddSeconds(120)
while ((Get-Date) -lt $deadline) { $script:p.Refresh(); if ($script:p.MainWindowHandle -ne [IntPtr]::Zero -and $script:p.MainWindowTitle -like "TSA*") { break }; Start-Sleep -Milliseconds 500 }
$script:w = $script:p.MainWindowHandle
Start-Sleep -Seconds 8
$wa = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
[Win]::ShowWindow($script:w, 9) | Out-Null
[Win]::MoveWindow($script:w, $wa.Left, $wa.Top, $wa.Width, $wa.Height, $true) | Out-Null
Start-Sleep -Seconds 3
$origTabs = (Tabs).Count
Say "1. projet ouvert : $origTabs onglet(s) [$(TabNames)] — titre « $((Fresh $script:w).Current.Name) » ; état « $((Texts 'nœuds .* élém') -join ' | ') »"
Shot "01_projet_ouvert.png"

# --- 2-4. Imports IFC2X3, IFC4, IFC4X3_ADD2 -------------------------------------------------------------------------
$n = 2
foreach ($f in "ifc2x3_Building-Structural.ifc", "ifc4_Building-Architecture.ifc", "ifc4x3_Building-Structural.ifc") {
    $before = (Tabs).Count
    $end = ImportIfc (Join-Path $Data $f)
    if ($end -and $end.Current.Name -eq "Rapport d'import IFC") {
        Say "$n. import $f : rapport « $((Texts 'produit' $end) -join ' ') »"
        $tabsTitles = (Find-All $end $CT::TabItem | ForEach-Object { $_.Current.Name }) -join " | "
        Say "    onglets du rapport : $tabsTitles"
        Capture (HandleOf $end) (Join-Path $OutDir ("rapport_" + [IO.Path]::GetFileNameWithoutExtension($f) + ".png")) | Out-Null
        ClickButtonIn $end "Fermer" | Out-Null
        Start-Sleep -Seconds 3
    } elseif ($end) { Say "$n. import $f : message « $(MessageText $end) »"; ClickButtonIn $end "OK" | Out-Null }
    else { Say "$n. import $f : aucune fin observée" }
    Say "    onglets : $before → $((Tabs).Count) [$(TabNames)]"
    Shot ("0${n}_import_" + [IO.Path]::GetFileNameWithoutExtension($f) + ".png")
    $n++
}
$importTabs = (Tabs).Count

# --- 5. Annulation ----------------------------------------------------------------------------------------------------
if ($BigFile -and (Test-Path $BigFile)) {
    $before = (Tabs).Count
    $end = ImportIfc $BigFile -Cancel
    if ($end) { Say "5. annulation : fenêtre de fin « $($end.Current.Name) » : $(MessageText $end)"; ClickButtonIn $end "Fermer" | Out-Null; ClickButtonIn $end "OK" | Out-Null }
    Start-Sleep -Seconds 2
    $status = (Texts "annulé|Annulation|importé") -join " | "
    Say "5. annulation : onglets $before → $((Tabs).Count) ; état « $status »"
    Shot "05_annulation.png"
} else { Say "5. annulation : grand fichier absent (-BigFile), étape non jouée" }

# --- 6. Fichier tronqué -----------------------------------------------------------------------------------------------
$before = (Tabs).Count
$end = ImportIfc $Truncated
if ($end) {
    Say "6. fichier tronqué : « $($end.Current.Name) » : $(MessageText $end)"
    Capture (HandleOf $end) (Join-Path $OutDir "06_message_tronque.png") | Out-Null
    ClickButtonIn $end "OK" | Out-Null
} else { Say "6. fichier tronqué : aucun message observé" }
Start-Sleep -Seconds 2
Say "    onglets : $before → $((Tabs).Count) [$(TabNames)]"

# --- 7. Couche IFC : masquer / réafficher (onglet du dernier import au premier plan) -------------------------------
$last = (Tabs) | Where-Object { $_.Current.Name -like "*ifc4x3_Building-Structural*" } | Select-Object -First 1
if ($last) { Click-Mouse $last; Start-Sleep -Seconds 3 }
$toggle = RibbonButton "Objets IFC import*"
if ($toggle) {
    $state = try { Toggle-State $toggle } catch { "?" }
    Say "7. bouton « $($toggle.Current.Name) » : état $state"
    Click-Mouse $toggle; Start-Sleep -Seconds 2
    Say "    après clic : $(try { Toggle-State $toggle } catch { '?' }) ; état « $((Texts 'Objets IFC') -join ' | ') »"
    Shot "07a_couche_ifc_masquee.png"
    Click-Mouse $toggle; Start-Sleep -Seconds 2
    Say "    après second clic : $(try { Toggle-State $toggle } catch { '?' }) ; état « $((Texts 'Objets IFC') -join ' | ') »"
    Shot "07b_couche_ifc_reaffichee.png"
} else { Say "7. bouton « Objets IFC importés » introuvable dans le ruban affiché" }

# --- 8. Survol et clic sur un objet importé ---------------------------------------------------------------------------
$r = New-Object Win+RECT; [Win]::GetWindowRect($script:w, [ref]$r) | Out-Null
$hit = $null
for ($fy = 0.35; $fy -le 0.85 -and -not $hit; $fy += 0.04) {
    for ($fx = 0.3; $fx -le 0.9 -and -not $hit; $fx += 0.03) {
        $x = [int]($r.Left + ($r.Right - $r.Left) * $fx); $y = [int]($r.Top + ($r.Bottom - $r.Top) * $fy)
        [MouseInput]::SetCursorPos($x, $y) | Out-Null; Start-Sleep -Milliseconds 120
        $hover = Texts "^Ifc\w+ «"
        if ($hover) { $hit = @($x, $y, ($hover -join ' ')) }
    }
}
if ($hit) {
    Say "8. survol : « $($hit[2]) »"
    [MouseInput]::Click($hit[0], $hit[1]); Start-Sleep -Seconds 2
    Say "    inspecteur : « $((Texts '^OBJET IFC') -join ' | ') »"
    $items = Find-All (Fresh $script:w) $CT::TreeItem | ForEach-Object { $_.Current.Name } | Where-Object { $_ -match "^(Identité|Calcul|Classe IFC|Étage|État|GlobalId|TSA_Import|Pset_|Materials|Parent|Géométrie|Classification)" }
    Say "    arbre des propriétés : $(($items | Select-Object -Unique) -join ' | ')"
    Capture $script:w (Join-Path $OutDir "08_inspecteur_objet_ifc.png") | Out-Null
} else { Say "8. aucun objet importé détecté au survol (grille de points)" }

# --- 9. Export ----------------------------------------------------------------------------------------------------------
$exp = RibbonButton "Exporter *IFC*"
if ($exp) {
    Click-Mouse $exp
    if (FillFileDialog "Exporter au format IFC" $Exported) {
        $end = WaitWindow { param($x) $x.Current.Name -eq "Export IFC" -and (Find-ByName $x "OK" $CT::Button) } 60
        if ($end) {
            Say "9. export : $(MessageText $end)"
            Capture (HandleOf $end) (Join-Path $OutDir "09_message_export.png") | Out-Null
            ClickButtonIn $end "OK" | Out-Null
        } else { Say "9. export : aucun message de fin" }
    }
} else { Say "9. bouton « Exporter IFC » introuvable" }
Say "    fichier exporté : $(if (Test-Path $Exported) { (Get-Item $Exported).Length.ToString() + ' octets' } else { 'absent' })"

# --- 10. Onglet d'origine : projet intact -------------------------------------------------------------------------------
$first = (Tabs) | Where-Object { $_.Current.Name -like "Projet_ouvert*" } | Select-Object -First 1
if ($first) {
    Click-Mouse $first; Start-Sleep -Seconds 3
    Say "10. onglet d'origine « $($first.Current.Name) » : état « $((Texts 'nœuds .* élém') -join ' | ') »"
    Shot "10_projet_origine_intact.png"
} else { Say "10. onglet d'origine introuvable" }

# --- Fermeture sans enregistrer, réglages restaurés ------------------------------------------------------------------
$closed = Close-App $script:p $script:w 60 $false
Say "fin : application fermée = $closed ; onglets IFC créés = $($importTabs - $origTabs)"
if (Test-Path $RegBackup) { & reg.exe delete "HKCU\Software\TSA Engineering" /f *> $null; & reg.exe import $RegBackup *> $null }
