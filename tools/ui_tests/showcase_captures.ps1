param(
    [string]$Exe = "",
    [string]$Projects = "",
    [Parameter(Mandatory = $true)][string]$OutDir,
    [string[]]$Only = @(),
    [int]$OrbitFrames = 24,
    [int]$AnimFrames = 12
)
# Captures de l'application TSA réelle sur les quatre projets de démonstration (examples/TSA_Projet_*.tsa, écrits par
# TSA_DEMO_DIR=<dossier> TSA_TestSuite --suite=demo), pour le site et la vidéo de présentation. Pour chaque projet :
# modèle physique au rendu texturé (Alt+1, Alt+5), charges masquées, série d'images en orbite (clic droit glissé),
# vue avec arêtes (Alt+8), calcul réel (F5), résultats et déformée, images de l'animation de la déformée (lecture du
# panneau Résultats), diagramme de contrainte normale σ et, s'il y a des câbles, effort normal N. Aucune retouche.
# TSA mémorise ses préférences dans le registre (HKCU\Software\TSA…) : les sauvegarder avant et les restaurer après.
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $Exe) { $Exe = Join-Path $here "..\..\build-ninja-release\dist\TSA\TSA.exe" }
if (-not $Projects) { $Projects = Join-Path $here "..\..\examples" }
$ErrorActionPreference = "Continue"
. (Join-Path $here "uia_lib.ps1")
New-Item -ItemType Directory -Force $OutDir | Out-Null
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class KbdShow {
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    public static void Press(byte[] vks) {
        foreach (var k in vks) keybd_event(k, 0, 0, UIntPtr.Zero);
        for (int i = vks.Length - 1; i >= 0; i--) keybd_event(vks[i], 0, 2, UIntPtr.Zero);
    }
}
"@
$VK = @{ Ctrl = 0x11; Alt = 0x12; Shift = 0x10; Num5 = 0x65; F = 0x46; L = 0x4C; D1 = 0x31; D5 = 0x35; D8 = 0x38; F5 = 0x74; F7 = 0x76 }
function Keys([string[]]$names) { Focus-Window $script:w; [KbdShow]::Press([byte[]]($names | ForEach-Object { $VK[$_] })); Start-Sleep -Milliseconds 1100 }
function View-Rect($w) {
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ClassNameProperty, "OccView")
    $v = (Fresh $w).FindFirst($TS::Descendants, $cond)
    if ($v) { return $v.Current.BoundingRectangle } else { return $null }
}
function Park { $s = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds; [MouseInput]::SetCursorPos($s.Width - 2, $s.Height - 2) | Out-Null; Start-Sleep -Milliseconds 500 }
function Clear-Hover($r) { [MouseInput]::SetCursorPos([int]($r.Left + $r.Width * 0.985), [int]($r.Top + $r.Height * 0.97)) | Out-Null; Start-Sleep -Milliseconds 400 }
function Shot($name) { Clear-Hover $script:r; Park; Capture $script:w (Join-Path $script:dir $name) | Out-Null; Write-Output "  capture : $name" }
function Zoom([double]$fx, [double]$fy, [int]$notches) {
    $r = $script:r
    [MouseInput]::SetCursorPos([int]($r.Left + $r.Width * $fx), [int]($r.Top + $r.Height * $fy)) | Out-Null; Start-Sleep -Milliseconds 200
    for ($i = 0; $i -lt [Math]::Abs($notches); $i++) { [MouseInput]::mouse_event(0x0800, 0, 0, [uint32]$(if ($notches -gt 0) { 120 } else { [uint32]::MaxValue - 119 }), [UIntPtr]::Zero); Start-Sleep -Milliseconds 120 }
    Start-Sleep -Milliseconds 600
}
function Orbit([int]$dx, [int]$dy) {
    $r = $script:r
    $x0 = [int]($r.Left + $r.Width / 2); $y0 = [int]($r.Top + $r.Height / 2)
    [MouseInput]::SetCursorPos($x0, $y0) | Out-Null; Start-Sleep -Milliseconds 120
    [MouseInput]::mouse_event(0x0008, 0, 0, 0, [UIntPtr]::Zero)
    $steps = 10
    for ($i = 1; $i -le $steps; $i++) { [MouseInput]::SetCursorPos([int]($x0 + $dx * $i / $steps), [int]($y0 + $dy * $i / $steps)) | Out-Null; Start-Sleep -Milliseconds 20 }
    [MouseInput]::mouse_event(0x0010, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 500
}
function Click-Dialog($proc, $titlePattern, $button, $seconds) {
    # Bouton invoqué par UI Automation (une fenêtre de progression peut recouvrir la boîte), jusqu'à sa fermeture
    $d = Find-Dialog $proc.Id $titlePattern $seconds
    if (-not $d) { return $false }
    for ($k = 0; $k -lt 6 -and $d; $k++) {
        $b = Find-ByName $d $button $CT::Button
        if ($b) { Click $b }
        Start-Sleep -Milliseconds 1200
        $d = Find-Dialog $proc.Id $titlePattern 1
    }
    return -not $d
}

$plans = @(
    @{ Key = "ba";      File = "TSA_Projet_Batiment_BA_R+4.tsa";          Zoom = 2; Cables = $false },
    @{ Key = "mixte";   File = "TSA_Projet_Batiment_Mixte_BA_Acier.tsa";  Zoom = 2; Cables = $false },
    @{ Key = "complexe"; File = "TSA_Projet_Batiment_Complexe_L.tsa";     Zoom = 2; Cables = $false },
    @{ Key = "pont";    File = "TSA_Projet_Pont_Haubans_80-180-80m.tsa";  Zoom = 1; Cables = $true }
)
foreach ($plan in $plans) {
    if ($Only.Count -gt 0 -and $Only -notcontains $plan.Key) { continue }
    $script:dir = Join-Path $OutDir $plan.Key
    New-Item -ItemType Directory -Force $script:dir | Out-Null
    Write-Output "== $($plan.File)"
    $p = Start-Process -FilePath $Exe -ArgumentList ('"' + (Join-Path $Projects $plan.File) + '"') -PassThru
    $deadline = (Get-Date).AddSeconds(120)
    while ((Get-Date) -lt $deadline) { $p.Refresh(); if ($p.MainWindowHandle -ne [IntPtr]::Zero -and $p.MainWindowTitle -like "TSA*Projet*") { break }; Start-Sleep -Milliseconds 500 }
    if ($p.MainWindowHandle -eq [IntPtr]::Zero) { Write-Output "  ÉCHEC : fenêtre introuvable"; continue }
    $script:w = $p.MainWindowHandle
    Start-Sleep -Seconds 6
    $wa = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
    [Win]::ShowWindow($script:w, 9) | Out-Null
    [Win]::MoveWindow($script:w, $wa.Left, $wa.Top, $wa.Width, $wa.Height, $true) | Out-Null
    Start-Sleep -Seconds 3
    $script:r = View-Rect $script:w
    Write-Output ("  vue 3D : {0} x {1}" -f [int]$script:r.Width, [int]$script:r.Height)

    # 1. Modèle physique, rendu texturé, charges masquées
    Keys @("Alt", "D1"); Keys @("Alt", "D5"); Keys @("Ctrl", "Shift", "L"); Keys @("F7"); Keys @("Num5"); Keys @("F")
    if ($plan.Zoom -gt 0) { Zoom 0.5 0.5 $plan.Zoom }
    Shot "physique.png"
    # 2. Orbite : rotations successives de la caméra (clic droit glissé), une capture par pas
    for ($i = 0; $i -lt $OrbitFrames; $i++) { Orbit 14 0; Clear-Hover $script:r; Park; Capture $script:w (Join-Path $script:dir ("orbite_{0:D2}.png" -f $i)) | Out-Null }
    Write-Output "  orbite : $OrbitFrames images"
    # 3. Arêtes des volumes
    Keys @("Num5"); Keys @("F"); if ($plan.Zoom -gt 0) { Zoom 0.5 0.5 $plan.Zoom }
    Keys @("Alt", "D8"); Start-Sleep 1; Shot "aretes.png"; Keys @("Alt", "D8")
    # 4. Calcul réel (F5) : avertissement dalles/voiles éventuel, puis « Calcul terminé »
    Keys @("F5")
    if (Click-Dialog $p "*Avertissements*" "Oui" 12) { Write-Output "  avertissement avant calcul : Oui" }
    if (Click-Dialog $p "*Calcul termin*" "OK" 120) { Write-Output "  calcul terminé : OK" } else { Write-Output "  ATTENTION : boîte « Calcul terminé » non trouvée" }
    Start-Sleep -Seconds 2
    Shot "resultats.png"
    # 5. Animation de la déformée (interpolation visuelle des résultats statiques)
    $play = Find-NameLike (Fresh $script:w) "*Lecture*" $CT::CheckBox
    if ($play) {
        Click $play; Park
        for ($i = 0; $i -lt $AnimFrames; $i++) { Capture $script:w (Join-Path $script:dir ("anim_{0:D2}.png" -f $i)) | Out-Null; Start-Sleep -Milliseconds 120 }
        $pause = Find-NameLike (Fresh $script:w) "*Pause*" $CT::CheckBox
        if ($pause) { Click $pause }
        Write-Output "  animation : $AnimFrames images"
    } else { Write-Output "  bouton Lecture introuvable" }
    # 6. Contrainte normale σ = N/A ± M/W
    if (Select-ComboItem $script:w "*Déformée 3D*" "*Contrainte normale*") { Start-Sleep 2; Shot "contraintes.png" } else { Write-Output "  diagramme σ non sélectionné" }
    # 7. Effort normal (câbles et haubans)
    if ($plan.Cables) {
        if (Select-ComboItem $script:w "*Contrainte normale*" "*Effort normal N*") { Start-Sleep 2; Shot "effort_normal.png" } else { Write-Output "  diagramme N non sélectionné" }
    }
    Close-App $p $script:w 40 $false | Out-Null
    Start-Sleep -Seconds 2
}
