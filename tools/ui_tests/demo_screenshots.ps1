param(
    [string]$Exe = "",
    [string]$Projects = "",
    [Parameter(Mandatory = $true)][string]$OutDir
)
# Captures d'écran de l'application TSA réelle sur les modèles de démonstration (examples/*.tsa, écrits par
# TSA_DEMO_DIR=<dossier> TSA_TestSuite --suite=demo). Pour chaque projet : ouverture dans TSA.exe (exécutable du
# paquet par défaut), fenêtre à la taille de l'écran, panneaux laissés tels quels, représentation « filaire
# analytique » (Alt+2), vues réglées par les commandes de TSA (raccourcis, rotation au clic droit, zoom à la
# molette), repère de survol effacé, capture de la fenêtre. TSA mémorise la liste des projets récents dans le
# registre (HKCU\Software\TSA Engineering) : la sauvegarder avant l'essai et la restaurer après si besoin.
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $Exe) { $Exe = Join-Path $here "..\..\build-ninja-release\dist\TSA\TSA.exe" }
if (-not $Projects) { $Projects = Join-Path $here "..\..\examples" }
$ErrorActionPreference = "Continue"
. (Join-Path $here "uia_lib.ps1")
New-Item -ItemType Directory -Force $OutDir | Out-Null
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class Kbd {
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    public static void Press(byte[] vks) {
        foreach (var k in vks) keybd_event(k, 0, 0, UIntPtr.Zero);
        for (int i = vks.Length - 1; i >= 0; i--) keybd_event(vks[i], 0, 2, UIntPtr.Zero);
    }
}
"@
$VK = @{ Ctrl = 0x11; Alt = 0x12; Num1 = 0x61; Num3 = 0x63; Num5 = 0x65; Num7 = 0x67; F = 0x46; D2 = 0x32 }
function Keys([string[]]$names) { Focus-Window $script:w; [Kbd]::Press([byte[]]($names | ForEach-Object { $VK[$_] })); Start-Sleep -Milliseconds 900 }

function Wait-ProcessWindow($proc, $pattern, $seconds = 90) {
    $deadline = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $deadline) {
        $proc.Refresh()
        if ($proc.MainWindowHandle -ne [IntPtr]::Zero -and $proc.MainWindowTitle -like $pattern) { return $proc.MainWindowHandle }
        Start-Sleep -Milliseconds 500
    }
    return [IntPtr]::Zero
}
function Wait-Tree($w) {
    for ($i = 0; $i -lt 40; $i++) {
        if ((Fresh $w).FindAll($TS::Descendants, [System.Windows.Automation.Condition]::TrueCondition).Count -gt 60) { return $true }
        Start-Sleep -Seconds 1
    }
    return $false
}
function View-Rect($w) {
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ClassNameProperty, "OccView")
    $v = (Fresh $w).FindFirst($TS::Descendants, $cond)
    if ($v) { return $v.Current.BoundingRectangle } else { return $null }
}
function Park-Cursor($w) {
    # Curseur sous la fenêtre (barre des tâches) : ni survol dans la vue, ni infobulle
    $s = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    [MouseInput]::SetCursorPos($s.Width - 2, $s.Height - 2) | Out-Null; Start-Sleep -Milliseconds 700
}
function Zoom($r, [double]$fx, [double]$fy, [int]$notches) {
    [MouseInput]::SetCursorPos([int]($r.Left + $r.Width * $fx), [int]($r.Top + $r.Height * $fy)) | Out-Null
    Start-Sleep -Milliseconds 200
    for ($i = 0; $i -lt [Math]::Abs($notches); $i++) {
        [MouseInput]::mouse_event(0x0800, 0, 0, [uint32]$(if ($notches -gt 0) { 120 } else { [uint32]::MaxValue - 119 }), [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 120
    }
    Start-Sleep -Milliseconds 500
}
function Orbit($r, [int]$dx, [int]$dy) {
    # Rotation de la caméra : clic droit glissé (OccView), départ au centre de la vue
    $x0 = [int]($r.Left + $r.Width / 2); $y0 = [int]($r.Top + $r.Height / 2)
    [MouseInput]::SetCursorPos($x0, $y0) | Out-Null; Start-Sleep -Milliseconds 150
    [MouseInput]::mouse_event(0x0008, 0, 0, 0, [UIntPtr]::Zero)
    $steps = 20
    for ($i = 1; $i -le $steps; $i++) { [MouseInput]::SetCursorPos([int]($x0 + $dx * $i / $steps), [int]($y0 + $dy * $i / $steps)) | Out-Null; Start-Sleep -Milliseconds 25 }
    [MouseInput]::mouse_event(0x0010, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 600
}
function Clear-Hover($r) {
    # Survol d'une zone vide de la vue (coin bas droit) : le repère d'accrochage du dernier survol disparaît
    [MouseInput]::SetCursorPos([int]($r.Left + $r.Width * 0.985), [int]($r.Top + $r.Height * 0.97)) | Out-Null
    Start-Sleep -Milliseconds 500
}
function Shot($name) { Clear-Hover $script:r; Park-Cursor $script:w; Capture $script:w (Join-Path $OutDir $name) | Out-Null; Write-Output "  capture : $name" }

$plans = @(
    @{ File = "TSA_Demo_Pont_Treillis_3x48m.tsa"; Views = {
        Keys @("Num5"); Orbit $script:r 170 25; Keys @("F"); Shot "pont_perspective.png"
        Keys @("Num1"); Keys @("F"); Shot "pont_elevation.png"
        Keys @("Num5"); Orbit $script:r 120 35; Keys @("F"); Zoom $script:r 0.50 0.50 9; Shot "pont_detail_treillis.png"
    } },
    @{ File = "TSA_Demo_Hangar_60x40m.tsa"; Views = {
        Keys @("Num5"); Keys @("F"); Shot "hangar_perspective.png"
        Keys @("Num5"); Orbit $script:r 60 170; Keys @("F"); Zoom $script:r 0.50 0.50 4; Shot "hangar_sous_toiture.png"
        Keys @("Num3"); Keys @("F"); Shot "hangar_portique_pignon.png"
        Keys @("Num5"); Orbit $script:r -150 20; Keys @("F"); Zoom $script:r 0.50 0.55 2; Shot "hangar_portiques_contreventements.png"
    } },
    @{ File = "TSA_Demo_Batiment_R+5.tsa"; Views = {
        Keys @("Num5"); Keys @("F"); Shot "batiment_perspective.png"
        Keys @("Num1"); Keys @("F"); Shot "batiment_facade.png"
        Keys @("Num5"); Orbit $script:r 180 -30; Keys @("F"); Zoom $script:r 0.55 0.50 4; Shot "batiment_niveaux_escalier.png"
    } }
)

foreach ($plan in $plans) {
    $path = Join-Path $Projects $plan.File
    Write-Output "== $($plan.File)"
    $p = Start-Process -FilePath $Exe -ArgumentList @("--lang=fr", ('"' + $path + '"')) -PassThru
    $h = Wait-ProcessWindow $p ("TSA*" + [IO.Path]::GetFileNameWithoutExtension($plan.File) + "*")
    if ($h -eq [IntPtr]::Zero) { Write-Output "  ÉCHEC : fenêtre introuvable"; continue }
    $script:w = $h
    Wait-Tree $h | Out-Null
    # Fenêtre à la taille de l'écran (hors barre des tâches) : la vue 3D la plus grande possible
    $wa = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
    [Win]::ShowWindow($h, 9) | Out-Null
    [Win]::MoveWindow($h, $wa.Left, $wa.Top, $wa.Width, $wa.Height, $true) | Out-Null
    Start-Sleep -Seconds 3
    $script:r = View-Rect $h
    Write-Output ("  vue 3D : {0} x {1}" -f [int]$script:r.Width, [int]$script:r.Height)
    Keys @("Alt", "D2")   # représentation filaire analytique
    & $plan.Views
    Close-App $p $h 40 $false | Out-Null
    Start-Sleep -Seconds 2
}
