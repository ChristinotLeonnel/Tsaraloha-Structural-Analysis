param(
    [string]$Exe = "",
    [Parameter(Mandatory = $true)][string]$Project,
    [string]$OutDir = (Join-Path $env:TEMP "tsa_ui_tests"),
    [int]$Width = 1220,
    [int]$Height = 765
)
if (-not $Exe) { $Exe = Join-Path $PSScriptRoot "..\..\build-ninja-debug\TSA.exe" }
# Taille réelle de la vue 3D (OccView) selon les panneaux ouverts, fenêtre de $Width × $Height pixels :
# disposition restaurée, panneau « Qualité du maillage » fermé, panneau « Propriétés » fermé ; calcul, captures.
$ErrorActionPreference = "Continue"
. "$PSScriptRoot\uia_lib.ps1"
New-Item -ItemType Directory -Force $OutDir | Out-Null
function View-Rect($w) {
    foreach ($e in (Fresh $w).FindAll($TS::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) {
        if ($e.Current.ClassName -eq "OccView") { return $e.Current.BoundingRectangle }
    }
    return $null
}
function Close-Dock($w, $title) {
    $t = Find-ByName (Fresh $w) $title
    if (-not $t) { return $false }
    $r = $t.Current.BoundingRectangle
    [MouseInput]::Click([int]($r.Right - 14), [int]($r.Top + 11)); Start-Sleep -Seconds 1
    return -not (Find-ByName (Fresh $w) $title)
}
function Report($w, $label, $file) {
    $v = View-Rect $w
    $win = (Fresh $w).Current.BoundingRectangle
    $share = if ($v) { [math]::Round(100.0 * $v.Width * $v.Height / ($win.Width * $win.Height), 1) } else { 0 }
    Write-Host ("{0} : fenêtre {1}×{2}, vue 3D {3}×{4} ({5} % de la fenêtre)" -f $label, $win.Width, $win.Height, $(if ($v) { $v.Width } else { "?" }), $(if ($v) { $v.Height } else { "?" }), $share)
    Capture $w "$OutDir\$file" | Out-Null
}

$p = Start-Process -FilePath $Exe -ArgumentList ('"' + $Project + '"') -PassThru
$we = Wait-Window ("TSA*" + [IO.Path]::GetFileName($Project) + "*") 90
$w = [IntPtr]$we.Current.NativeWindowHandle
Start-Sleep -Seconds 4
[Win]::ShowWindow($w, 9) | Out-Null
[Win]::MoveWindow($w, 0, 0, $Width, $Height, $true) | Out-Null
Focus-Window $w; Start-Sleep -Seconds 3
for ($k = 0; $k -lt 30 -and -not (View-Rect $w); $k++) { Start-Sleep -Milliseconds 500 }
$done = $null
for ($attempt = 1; $attempt -le 3 -and -not $done; $attempt++) {
    Focus-Window $w; Press "{F5}"
    $deadline = (Get-Date).AddSeconds(20)
    while (-not $done -and (Get-Date) -lt $deadline) {
        $pre = Find-Dialog $p.Id "Avertissements avant calcul*" 1
        if ($pre) { Click (Find-NameLike $pre "*Oui*" $CT::Button) }
        $d = Find-Dialog $p.Id "Calcul termin*" 1
        if ($d) { $done = $d.Current.Name; Click (Find-NameLike $d "*OK*" $CT::Button) }
    }
}
Write-Host ("calcul : " + $done)
Start-Sleep -Seconds 2
$fit = Find-NameLike (Fresh $w) "*Déformée" $CT::Button
if ($fit) { Click $fit; Start-Sleep -Seconds 2 }
Report $w "a. disposition restaurée" "v1_disposition_restauree.png"
$c1 = Close-Dock $w "Qualité du maillage"
if ($fit) { Click (Find-NameLike (Fresh $w) "*Déformée" $CT::Button); Start-Sleep -Seconds 2 }
Report $w ("b. panneau « Qualité du maillage » fermé (" + $c1 + ")") "v2_sans_qualite.png"
$c2 = Close-Dock $w "PROPRIÉTÉS"
if (-not $c2) { $c2 = Close-Dock $w "Propriétés" }
if ($fit) { Click (Find-NameLike (Fresh $w) "*Déformée" $CT::Button); Start-Sleep -Seconds 2 }
Report $w ("c. panneau « Propriétés » fermé aussi (" + $c2 + ")") "v3_sans_proprietes.png"
Stop-Process -Id $p.Id -Force
