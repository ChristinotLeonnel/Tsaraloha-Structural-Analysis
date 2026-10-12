# Onglets multi-projets dans l'application réelle : deux fichiers de même nom (dossiers d1, d2), un troisième projet
# calculé, isolement des résultats entre onglets, fichier déjà ouvert (pas de second onglet), Nouveau projet (onglet
# vide), copier dans un onglet et coller à la position d'origine dans un autre (indicateur ●), Ctrl+Tab /
# Ctrl+Maj+Tab, fermeture d'un onglet modifié (Annuler puis Ne pas enregistrer), fermeture de l'application.
# Les projets sont des copies des exemples placées dans -Work (chemin long : « ~ » est une touche pour SendKeys).
# Le scénario modifie les réglages de TSA (dossier du dernier projet, projets récents) : sauvegarder
# HKCU\Software\TSA* avant l'essai et les restaurer après.
param([string]$Exe = "", [Parameter(Mandatory = $true)][string]$Work)
# $PSScriptRoot est vide dans les valeurs par défaut de param() sous PowerShell 5.1
if (-not $Exe) { $Exe = Join-Path $PSScriptRoot "..\..\build-ninja-debug\TSA.exe" }
$ErrorActionPreference = "Continue"
. (Join-Path $PSScriptRoot "uia_lib.ps1")
$OutDir = Join-Path $Work "shots"
New-Item -ItemType Directory -Force $OutDir, (Join-Path $Work "d1"), (Join-Path $Work "d2"), (Join-Path $Work "neuf") | Out-Null
$A1 = Join-Path $Work "d1\Reservoir.tsa"; $A2 = Join-Path $Work "d2\Reservoir.tsa"; $B = Join-Path $Work "Dalle.tsa"
Copy-Item (Join-Path $PSScriptRoot "..\..\examples\TSA_Exemple_Voile_Reservoir.tsa") $A1 -Force
Copy-Item (Join-Path $PSScriptRoot "..\..\examples\TSA_Exemple_Voile_Reservoir.tsa") $A2 -Force
Copy-Item (Join-Path $PSScriptRoot "..\..\examples\TSA_Exemple_Dalle_Coques.tsa") $B -Force
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class KbdT {
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    public static void Press(byte[] vks) {
        foreach (var k in vks) keybd_event(k, 0, 0, UIntPtr.Zero);
        for (int i = vks.Length - 1; i >= 0; i--) keybd_event(vks[i], 0, 2, UIntPtr.Zero);
    }
}
"@
$VK = @{ Ctrl = 0x11; Alt = 0x12; Shift = 0x10; Tab = 0x09; Esc = 0x1B; A = 0x41; C = 0x43; N = 0x4E; O = 0x4F; V = 0x56; F5 = 0x74 }
function Keys([string[]]$names) { Focus-Window $script:w; [KbdT]::Press([byte[]]($names | ForEach-Object { $VK[$_] })); Start-Sleep -Milliseconds 1300 }
function Park { $s = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds; [MouseInput]::SetCursorPos($s.Width - 2, $s.Height - 2) | Out-Null; Start-Sleep -Milliseconds 500 }
function Shot($name) { Park; Capture $script:w (Join-Path $OutDir $name) | Out-Null; Write-Output "  capture : $name" }
function Tabs { @(Find-All (Fresh $script:w) $CT::TabItem | Where-Object { $_.Current.Name -match "\.tsa|Nouveau|Projet" }) }
function TabState {
    $t = Tabs
    $names = $t | ForEach-Object {
        $sel = $false
        try { $sel = $_.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Current.IsSelected } catch {}
        if ($sel) { "[" + $_.Current.Name + "]" } else { $_.Current.Name }
    }
    return "$($t.Count) onglet(s) : " + ($names -join " | ")
}
function DockTexts($pattern) {
    $out = @()
    foreach ($t in (Find-All (Fresh $script:w) $CT::Text)) { $n = $t.Current.Name; if ($n -match $pattern) { $out += $n } }
    return ($out | Select-Object -Unique) -join " | "
}
function AnswerDialogs($proc, $seconds) {
    $deadline = (Get-Date).AddSeconds($seconds)
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $proc.Id)
    $winCond = New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $CT::Window)
    $done = $false
    while ((Get-Date) -lt $deadline) {
        $found = $false
        foreach ($top in $AE::RootElement.FindAll($TS::Children, $cond)) {
            foreach ($win in @($top.FindAll($TS::Descendants, $winCond))) {
                $title = $win.Current.Name
                if ($title -notmatch "Calcul|Maillage|Avertissement|OpenSees|Charges|Appuis") { continue }
                foreach ($b in (Find-All $win $CT::Button)) { if ($b.Current.Name -in @("Oui", "Yes", "OK")) { Click $b; $found = $true; break } }
                if ($found) { break }
            }
            if ($found) { break }
        }
        if ($found) { $done = $true; Start-Sleep -Milliseconds 800 } elseif ($done) { break } else { Start-Sleep -Milliseconds 700 }
    }
}
function OpenFile($path) {
    Keys @("Ctrl", "O")
    $dlg = Find-Dialog $script:p.Id "*uvrir*" 30
    if (-not $dlg) { Write-Output "  dialogue Ouvrir introuvable"; return }
    $edit = $null
    foreach ($e in (Find-All $dlg $CT::Edit)) { if ($e.Current.Name -like "Nom du fichier*" -or $e.Current.Name -like "File name*") { $edit = $e; break } }
    if ($edit) { try { $edit.SetFocus() } catch {} }
    Start-Sleep -Milliseconds 500
    $escaped = [regex]::Replace($path, '[+^%~(){}\[\]]', { param($m) '{' + $m.Value + '}' })
    Press "^a"; Press $escaped; Start-Sleep -Milliseconds 500
    Press "{ENTER}"
    $t0 = Get-Date
    while ((Find-Dialog $script:p.Id "*uvrir*" 1) -and ((Get-Date) - $t0).TotalSeconds -lt 10) { Start-Sleep -Milliseconds 500 }
    if (Find-Dialog $script:p.Id "*uvrir*" 1) { Write-Output "  dialogue Ouvrir toujours affiché : Échap"; Press "{ESC}" }
    Start-Sleep -Seconds 8
}

$script:p = Start-Process -FilePath $Exe -ArgumentList ('"' + $A1 + '"') -PassThru
$deadline = (Get-Date).AddSeconds(120)
while ((Get-Date) -lt $deadline) { $script:p.Refresh(); if ($script:p.MainWindowHandle -ne [IntPtr]::Zero -and $script:p.MainWindowTitle -like "TSA*") { break }; Start-Sleep -Milliseconds 500 }
$script:w = $script:p.MainWindowHandle
Start-Sleep -Seconds 8
$wa = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
[Win]::ShowWindow($script:w, 9) | Out-Null
[Win]::MoveWindow($script:w, $wa.Left, $wa.Top, $wa.Width, $wa.Height, $true) | Out-Null
Start-Sleep -Seconds 3
Write-Output "1. lancement : $(TabState) — titre « $((Fresh $script:w).Current.Name) »"

OpenFile $A2
Write-Output "2. même nom, autre dossier : $(TabState)"
OpenFile $B
Write-Output "3. troisième projet : $(TabState)"

Keys @("F5")
AnswerDialogs $script:p 120
Start-Sleep -Seconds 3
Write-Output ("4. calcul dans Dalle : état « " + (DockTexts "Calcul disponible|Aucun résultat|calculé|Réactions") + " »")
Shot "t01_dalle_calculee.png"

$first = (Tabs)[0]
Click-Mouse $first; Start-Sleep -Seconds 3
Write-Output ("5. onglet 1 activé : $(TabState) ; état des résultats « " + (DockTexts "Calcul disponible|Aucun résultat|calculé") + " »")
Shot "t02_reservoir_sans_resultat.png"

OpenFile $A1
Write-Output "6. fichier déjà ouvert rouvert : $(TabState)"

Keys @("Ctrl", "A"); Keys @("Ctrl", "C")
Write-Output ("7. copie : « " + (DockTexts "Presse-papier") + " »")

Keys @("Ctrl", "N")
$nd = Find-Dialog $script:p.Id "Nouveau projet" 15
if ($nd) {
    $edits = @(Find-All $nd $CT::Edit)
    foreach ($e in $edits) { if ((Value-Of $e) -like "*:\*") { Set-Value $e (Join-Path $Work "neuf") } }
    $create = Find-ByName (Fresh $nd) "Créer" $CT::Button
    if ($create) { Click $create }
    Start-Sleep -Seconds 8
} else { Write-Output "  dialogue Nouveau projet introuvable" }
Write-Output ("8. nouveau projet : $(TabState) ; état « " + (DockTexts "Calcul disponible|Aucun résultat|calculé") + " »")
Shot "t03_nouveau_projet_vide.png"

Keys @("Ctrl", "Shift", "V"); Start-Sleep -Seconds 3
Write-Output ("9. collage à l'origine : « " + (DockTexts "Collé") + " » ; $(TabState)")
Shot "t04_colle_depuis_reservoir.png"

Keys @("Ctrl", "Tab")
Write-Output "10. Ctrl+Tab : $(TabState)"
Keys @("Ctrl", "Shift", "Tab")
Write-Output "11. Ctrl+Maj+Tab : $(TabState)"

# Fermeture de l'onglet modifié (●) par son bouton : confirmation, « Annuler » d'abord, puis « Ne pas tenir compte »
$cur = (Tabs) | Where-Object { $_.Current.Name -like ("*" + [char]0x25CF) } | Select-Object -First 1
if ($cur) { Click-Mouse $cur; Start-Sleep -Seconds 2; Write-Output "    onglet modifié activé : $(TabState)"; $cur = (Tabs) | Where-Object { $_.Current.Name -like ("*" + [char]0x25CF) } | Select-Object -First 1 }
if ($cur) {
    $r = $cur.Current.BoundingRectangle
    $closeBtn = @(Find-All $cur $CT::Button) | Select-Object -First 1
    function ClickClose { if ($closeBtn) { Click-Mouse $closeBtn } else { [MouseInput]::SetCursorPos([int]($r.Right - 10), [int]($r.Top + $r.Height / 2)) | Out-Null; [MouseInput]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); [MouseInput]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero) } }
    foreach ($answer in @("Annuler", "Ne pas")) {
        ClickClose; Start-Sleep -Seconds 2
        $box = Find-Dialog $script:p.Id "*nregistrer*" 8
        if ($box) {
            $b = @(Find-All $box $CT::Button) | Where-Object { $_.Current.Name -like "$answer*" } | Select-Object -First 1
            Write-Output ("12. confirmation « " + $box.Current.Name + " » → $answer")
            if ($b) { Click-Mouse $b }
            Start-Sleep -Seconds 3
        } else { Write-Output "12. aucune confirmation" }
        Write-Output "    après « $answer » : $(TabState)"
    }
}

$closed = Close-App $script:p $script:w 60 $false
Write-Output "13. application fermée (tous les onglets) : $closed"
