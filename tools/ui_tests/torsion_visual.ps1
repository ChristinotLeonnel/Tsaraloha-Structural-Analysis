param(
    [string]$Exe = "",
    [Parameter(Mandatory = $true)][string]$Project,
    [string]$OutDir = (Join-Path $env:TEMP "tsa_ui_tests")
)
if (-not $Exe) { $Exe = Join-Path $PSScriptRoot "..\..\build-ninja-debug\TSA.exe" }
$ErrorActionPreference = "Continue"
Add-Type @"
using System.Runtime.InteropServices;
public static class Dpi { [DllImport("user32.dll")] public static extern bool SetProcessDPIAware(); }
"@
[Dpi]::SetProcessDPIAware() | Out-Null
. "$PSScriptRoot\uia_lib.ps1"
$exe = $Exe
New-Item -ItemType Directory -Force $OutDir | Out-Null
$dir = $OutDir
$p = Start-Process -FilePath $exe -ArgumentList @("--lang=fr", ('"' + $Project + '"')) -PassThru
$we = Wait-Window "TSA*torsion_ui*" 90
$w = [IntPtr]$we.Current.NativeWindowHandle
Start-Sleep -Seconds 4
[Win]::ShowWindow($w, 3) | Out-Null
Focus-Window $w; Start-Sleep -Seconds 3
# Panneau « Qualité du maillage » restauré d'une session précédente : fermé par un clic réel sur la croix de
# sa barre de titre (même ligne que le titre, bord droit du panneau).
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class Mouse {
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    public static void Click(int x, int y) { SetCursorPos(x, y); mouse_event(2, 0, 0, 0, UIntPtr.Zero); mouse_event(4, 0, 0, 0, UIntPtr.Zero); }
}
"@
$closed = $false
$title = $null
for ($k = 0; $k -lt 40 -and -not $title; $k++) { $title = Find-ByName (Fresh $w) "Qualité du maillage"; if (-not $title) { Start-Sleep -Milliseconds 500; if ($k % 8 -eq 7) { Focus-Window $w } } }
if ($title) {
    $r = $title.Current.BoundingRectangle
    $win = (Fresh $w).Current.BoundingRectangle
    $x = [int]($r.Right - 14); $y = [int]($r.Top + 11)
    Write-Host ("titre du panneau : " + $title.Current.ControlType.ProgrammaticName + " " + $r + " ; clic en " + $x + "," + $y)
    [Mouse]::Click($x, $y); Start-Sleep -Seconds 2
    $closed = -not (Find-ByName (Fresh $w) "Qualité du maillage")
}
Write-Host ("panneau de qualité fermé : " + $closed)
Focus-Window $w
Press "{F5}"
$deadline = (Get-Date).AddSeconds(60); $done = $null
while (-not $done -and (Get-Date) -lt $deadline) { $d = Find-Dialog $p.Id "Calcul termin*" 2; if ($d) { $done = $d.Current.Name; $b = Find-NameLike $d "*OK*" $CT::Button; Click $b } }
Write-Host ("calcul : " + $done)
Start-Sleep -Seconds 2
$view = Find-NameLike (Fresh $w) "*" $CT::Custom
$best = $null; $area = 0
$all = @(Find-All (Fresh $w) $CT::Custom) + @(Find-All (Fresh $w) $CT::Pane)
Write-Host ("classes : " + (($all | ForEach-Object { $_.Current.ClassName } | Select-Object -Unique) -join ", "))
foreach ($c in $all) {
    $rc = $c.Current.BoundingRectangle
    if ($c.Current.ClassName -like "*OccView*" -and $rc.Width * $rc.Height -gt $area) { $best = $c; $area = $rc.Width * $rc.Height }
}
if ($best) {
    $rc = $best.Current.BoundingRectangle
    Write-Host ("vue 3D : " + $rc)
    [Mouse]::Click([int]($rc.Left + 8), [int]($rc.Bottom - 8)); Start-Sleep -Milliseconds 500
}
Press "f"; Start-Sleep -Seconds 2
$fitDef = Find-NameLike (Fresh $w) "*Déformée" $CT::Button
Write-Host ("bouton cadrer la déformée : " + $(if ($fitDef) { $fitDef.Current.Name } else { "absent" }))
if ($fitDef) { Click $fitDef; Start-Sleep -Seconds 2 }
Write-Host (Capture $w "$dir\20_app_torsion_iso.png")
$chk = Find-ByName (Fresh $w) "Rotation des sections (torsion)" $CT::CheckBox
Do-Toggle $chk; Start-Sleep -Seconds 2
Write-Host (Capture $w "$dir\21_app_sans_rotation.png")
$chk = Find-ByName (Fresh $w) "Rotation des sections (torsion)" $CT::CheckBox
Do-Toggle $chk; Start-Sleep -Seconds 2
Stop-Process -Id $p.Id -Force
