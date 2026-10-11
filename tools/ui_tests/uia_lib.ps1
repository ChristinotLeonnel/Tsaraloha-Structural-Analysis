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
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class MouseInput {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    public static void Click(int x, int y) { SetCursorPos(x, y); mouse_event(2, 0, 0, 0, UIntPtr.Zero); mouse_event(4, 0, 0, 0, UIntPtr.Zero); }
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
}
"@
# Coordonnées UI Automation et curseur dans le même repère (pixels physiques).
[MouseInput]::SetProcessDPIAware() | Out-Null
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
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $processId)
    $winCond = New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $CT::Window)
    while ((Get-Date) -lt $deadline) {
        try {
            foreach ($top in $AE::RootElement.FindAll($TS::Children, $cond)) {
                if ($top.Current.Name -like $titlePattern -and $top.Current.ControlType -eq $CT::Window) { return $top }
                foreach ($child in $top.FindAll($TS::Children, $winCond)) { if ($child.Current.Name -like $titlePattern) { return $child } }
            }
        } catch {}
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
function Click-Mouse($e) {
    $r = $e.Current.BoundingRectangle
    [MouseInput]::Click([int]($r.Left + $r.Width / 2), [int]($r.Top + $r.Height / 2)); Start-Sleep -Milliseconds 600
}
function Click($e) {
    # Invoke ; s'il ne rend pas la main (action Qt synchrone, boîte modale…), vrai clic souris au centre.
    try { $e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke() }
    catch { try { Click-Mouse $e } catch {} }
    Start-Sleep -Milliseconds 500
}
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
function Close-App($proc, $w, $seconds = 40, [bool]$save = $false) {
    # Fermeture de la fenêtre, puis réponse « ne pas enregistrer » à toute boîte de confirmation du processus ; vrai si le
    # processus s'est terminé.
    # WM_CLOSE posté (comme la croix de la fenêtre) : indépendant du focus clavier, ne bloque pas.
    [MouseInput]::PostMessage((HandleOf $w), 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    $deadline = (Get-Date).AddSeconds($seconds)
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $proc.Id)
    $winCond = New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $CT::Window)
    while (-not $proc.HasExited -and (Get-Date) -lt $deadline) {
        try {
            foreach ($top in $AE::RootElement.FindAll($TS::Children, $cond)) {
                foreach ($win in @($top) + @($top.FindAll($TS::Descendants, $winCond))) {
                    foreach ($b in (Find-All $win $CT::Button)) {
                        $n = $b.Current.Name
                        # Boîte « TSA - Enregistrer les modifications » : Enregistrer / Annuler / Ne pas tenir compte.
                        $isPrompt = $win.Current.Name -like "*Enregistrer les modifications*" -or $win.Current.Name -like "*enregistrer*"
                        $wanted = if ($save) { $isPrompt -and $n -eq "Enregistrer" } else { $n -like "Ne pas tenir compte*" -or $n -like "*Ne pas enregistrer*" -or $n -eq "Non" -or $n -eq "No" -or $n -like "Discard*" }
                        if ($wanted) {
                            Write-Host ("  fermeture : « " + $win.Current.Name + " » → " + $n); Click-Mouse $b
                        }
                    }
                }
            }
        } catch {}
        Start-Sleep -Milliseconds 700
        $proc.Refresh()
    }
    return $proc.HasExited
}
function Save-Project($w) {
    # Bouton « Enregistrer » du ruban, cliqué à la souris (Ctrl+S dépend du focus clavier).
    $b = Find-ByName (Fresh $w) "Enregistrer" $CT::Button
    if ($b) { Click-Mouse $b; Start-Sleep -Seconds 2; return $true }
    Focus-Window $w; Press "^s"; Start-Sleep -Seconds 2; return $false
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
