# Bibliothèque d'essai de l'interface TSA par UI Automation (Windows) : lancement, recherche de contrôles,
# actions réelles (bascule, sélection, saisie, touches), captures de la fenêtre.
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms, System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int hgt, bool repaint);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$CT = [System.Windows.Automation.ControlType]

function Find-All($root, $controlType) {
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $controlType)
    return $root.FindAll($TS::Descendants, $cond)
}
function Find-ByName($root, $name, $controlType = $null) {
    $c1 = New-Object System.Windows.Automation.PropertyCondition($AE::NameProperty, $name)
    if ($controlType) {
        $c2 = New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $controlType)
        $c1 = New-Object System.Windows.Automation.AndCondition($c1, $c2)
    }
    return $root.FindFirst($TS::Descendants, $c1)
}
function Find-NameLike($root, $pattern, $controlType) {
    foreach ($e in (Find-All $root $controlType)) { if ($e.Current.Name -like $pattern) { return $e } }
    return $null
}
function Wait-Window($titlePattern, $seconds = 60) {
    $deadline = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $deadline) {
        $wins = $AE::RootElement.FindAll($TS::Children, [System.Windows.Automation.Condition]::TrueCondition)
        foreach ($w in $wins) { if ($w.Current.Name -like $titlePattern) { return $w } }
        Start-Sleep -Milliseconds 500
    }
    return $null
}
function Find-Dialog($processId, $titlePattern, $seconds = 10) {
    $deadline = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $deadline) {
        $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $processId)
        foreach ($w in $AE::RootElement.FindAll($TS::Descendants, $cond)) {
            if ($w.Current.ControlType -eq $CT::Window -and $w.Current.Name -like $titlePattern) { return $w }
        }
        Start-Sleep -Milliseconds 300
    }
    return $null
}
function Focus-Window($w) {
    $h = HandleOf $w
    [Win]::ShowWindow($h, 9) | Out-Null
    [Win]::SetForegroundWindow($h) | Out-Null
    Start-Sleep -Milliseconds 300
}
function Press($keys) { [System.Windows.Forms.SendKeys]::SendWait($keys); Start-Sleep -Milliseconds 300 }
function Toggle-State($e) { return $e.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern).Current.ToggleState }
function Do-Toggle($e) { $e.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern).Toggle(); Start-Sleep -Milliseconds 800 }
function Click($e) { $e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke(); Start-Sleep -Milliseconds 500 }
function Value-Of($e) {
    try { return $e.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).Current.Value } catch { return $e.Current.Name }
}
function Set-Value($e, $v) { $e.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).SetValue($v); Start-Sleep -Milliseconds 800 }
function HandleOf($w) { if ($w -is [IntPtr]) { return $w } else { return [IntPtr]$w.Current.NativeWindowHandle } }
function Fresh($w) { return $AE::FromHandle((HandleOf $w)) }
function Find-Combo($w, $valuePattern) {
    foreach ($c in (Find-All (Fresh $w) $CT::ComboBox)) { if ((Value-Of $c) -like $valuePattern) { return $c } }
    return $null
}
function Select-ComboItem($w, $currentPattern, $itemPattern) {
    # Liste dépliée (le menu déroulant prend le focus clavier), position de l'élément sélectionné et de
    # l'élément visé lues par UI Automation, déplacement par flèches puis Entrée ; valeur relue ensuite.
    $combo = $null
    for ($k = 0; $k -lt 20 -and -not $combo; $k++) { $combo = Find-Combo $w $currentPattern; if (-not $combo) { Start-Sleep -Milliseconds 500 } }
    if (-not $combo) { return $false }
    Focus-Window $w
    $ec = $combo.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
    $ec.Expand(); Start-Sleep -Milliseconds 900
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $CT::ListItem)
    $items = @($combo.FindAll($TS::Descendants, $cond))
    $from = -1; $to = -1
    for ($i = 0; $i -lt $items.Count; $i++) {
        $n = $items[$i].Current.Name
        if ($from -lt 0 -and $n -like $currentPattern) { $from = $i }
        if ($to -lt 0 -and $n -like $itemPattern) { $to = $i }
    }
    if ($from -lt 0 -or $to -lt 0) { try { $ec.Collapse() } catch {}; return $false }
    $delta = $to - $from
    for ($i = 0; $i -lt [Math]::Abs($delta); $i++) { if ($delta -gt 0) { Press "{DOWN}" } else { Press "{UP}" } }
    Press "{ENTER}"
    Start-Sleep -Milliseconds 1200
    return [bool](Find-Combo $w $itemPattern)
}
function Capture($w, $path) {
    Focus-Window $w
    $r = New-Object Win+RECT
    [Win]::GetWindowRect((HandleOf $w), [ref]$r) | Out-Null
    $width = $r.Right - $r.Left; $height = $r.Bottom - $r.Top
    $bmp = New-Object System.Drawing.Bitmap $width, $height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save($path)
    $g.Dispose(); $bmp.Dispose()
    return "$path (${width}x${height})"
}
