# Glisser-déposer de fichiers depuis l'Explorateur (vrai glisser OLE à la souris) et « Répéter la dernière commande »
# (Ctrl+Entrée) avec plusieurs onglets, dans l'application réelle.
#   Dépôt : second fichier de même nom (autre dossier), troisième projet, fichier déjà ouvert, fichier corrompu,
#   fichier verrouillé par un autre processus, fichier non .tsa ; les onglets ouverts sont conservés.
#   Ctrl+Entrée : « Dessiner Poutre » (B) lancé dans un onglet, puis Ctrl+Entrée après changement d'onglet et après
#   fermeture de l'onglet d'origine ; l'outil doit s'activer dans l'onglet visible seulement.
# Lignes « OK » / « ÉCHEC » et bilan. Les projets sont des copies des exemples placées dans -Work (chemin long).
# Le scénario modifie les réglages de TSA (projets récents) : sauvegarder HKCU\Software\TSA* avant, restaurer après.
param([string]$Exe = "", [Parameter(Mandatory = $true)][string]$Work)
# $PSScriptRoot est vide dans les valeurs par défaut de param() sous PowerShell 5.1
if (-not $Exe) { $Exe = Join-Path $PSScriptRoot "..\..\build-ninja-debug\TSA.exe" }
$ErrorActionPreference = "Continue"
. (Join-Path $PSScriptRoot "uia_lib.ps1")
$OutDir = Join-Path $Work "shots"
New-Item -ItemType Directory -Force $OutDir, (Join-Path $Work "d1"), (Join-Path $Work "d2") | Out-Null
$A1 = Join-Path $Work "d1\Reservoir.tsa"; $A2 = Join-Path $Work "d2\Reservoir.tsa"; $B = Join-Path $Work "Dalle.tsa"
Copy-Item (Join-Path $PSScriptRoot "..\..\examples\TSA_Exemple_Voile_Reservoir.tsa") $A1 -Force
Copy-Item (Join-Path $PSScriptRoot "..\..\examples\TSA_Exemple_Voile_Reservoir.tsa") $A2 -Force
Copy-Item (Join-Path $PSScriptRoot "..\..\examples\TSA_Exemple_Dalle_Coques.tsa") $B -Force
[System.IO.File]::WriteAllBytes((Join-Path $Work "Corrompu.tsa"), [byte[]](1..2000 | ForEach-Object { ($_ * 37) % 256 }))
Copy-Item $B (Join-Path $Work "Verrouille.tsa") -Force
"pas un projet" | Set-Content (Join-Path $Work "Notes.txt")

Add-Type @"
using System; using System.Runtime.InteropServices;
public static class KbdD {
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    public static void Press(byte[] vks) {
        foreach (var k in vks) keybd_event(k, 0, 0, UIntPtr.Zero);
        for (int i = vks.Length - 1; i >= 0; i--) keybd_event(vks[i], 0, 2, UIntPtr.Zero);
    }
}
"@
$VK = @{ Ctrl = 0x11; Shift = 0x10; Tab = 0x09; Esc = 0x1B; Enter = 0x0D; B = 0x42; W = 0x57 }
$script:ok = 0; $script:ko = 0
function Check($cond, $label) { if ($cond) { $script:ok++; Write-Output "OK     $label" } else { $script:ko++; Write-Output "ÉCHEC  $label" } }
function Keys([string[]]$names) { Focus-Window $script:w; [KbdD]::Press([byte[]]($names | ForEach-Object { $VK[$_] })); Start-Sleep -Milliseconds 1300 }
function Shot($name) { Capture $script:w (Join-Path $OutDir $name) | Out-Null }
function Tabs { @(Find-All (Fresh $script:w) $CT::TabItem | Where-Object { $_.Current.Name -match "\.tsa" }) }
function IsSel($t) { try { $t.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Current.IsSelected } catch { $false } }
function TabState { $t = Tabs; return "$($t.Count) onglet(s) : " + (($t | ForEach-Object { if (IsSel $_) { "[" + $_.Current.Name + "]" } else { $_.Current.Name } }) -join " | ") }
function CurrentTab { (Tabs) | Where-Object { IsSel $_ } | Select-Object -First 1 | ForEach-Object { $_.Current.Name } }
function BeamToolOn {
    # Outil « Dessiner Poutre » actif dans l'onglet visible : consigne de la barre d'état de cet onglet (les widgets
    # des onglets cachés sont hors écran). Le bouton du ruban n'expose pas son état coché à UI Automation.
    foreach ($t in (Find-All (Fresh $script:w) $CT::Text)) {
        if (-not $t.Current.IsOffscreen -and $t.Current.Name -like "*1er n*ud*") { return $true }
    }
    return $false
}
function ToolPanels {
    # Panneaux « Éléments Filaires » visibles (fenêtres d'outil des documents)
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $script:p.Id)
    $winCond = New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $CT::Window)
    return @(foreach ($top in $AE::RootElement.FindAll($TS::Children, $cond)) {
        foreach ($win in $top.FindAll($TS::Descendants, $winCond)) { if ($win.Current.Name -like "*l*ments Filaires*" -and -not $win.Current.IsOffscreen) { $win } } })
}
function CloseToolPanels { foreach ($pn in (ToolPanels)) { $b = @(Find-All $pn $CT::Button) | Where-Object { $_.Current.Name -like "*Fermer*" } | Select-Object -First 1; if ($b) { Click-Mouse $b; Start-Sleep -Milliseconds 800 } } }
function WaitTabs($n) {
    # L'arbre UIA de Qt est rafraîchi avec retard : relecture jusqu'à stabilisation (10 s au plus)
    $deadline = (Get-Date).AddSeconds(10)
    while ((Get-Date) -lt $deadline) { if ((Tabs).Count -eq $n) { return $true }; Start-Sleep -Milliseconds 500 }
    return $false
}
function DismissDialogs($seconds) {
    # Boîtes modales du processus TSA (erreur d'ouverture…) : texte relevé, puis OK / Fermer
    $deadline = (Get-Date).AddSeconds($seconds); $texts = @()
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $script:p.Id)
    $winCond = New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $CT::Window)
    while ((Get-Date) -lt $deadline) {
        $found = $false
        foreach ($top in $AE::RootElement.FindAll($TS::Children, $cond)) {
            foreach ($win in @($top.FindAll($TS::Children, $winCond))) {
                $t = (Find-All $win $CT::Text | ForEach-Object { $_.Current.Name }) -join " / "
                $texts += ("« " + $win.Current.Name + " » : " + $t)
                $btn = @(Find-All $win $CT::Button) | Where-Object { $_.Current.Name -in @("OK", "Fermer", "Close") } | Select-Object -First 1
                if ($btn) { Click-Mouse $btn; $found = $true; Start-Sleep -Milliseconds 800 }
            }
        }
        if (-not $found -and $texts.Count -gt 0) { break }
        Start-Sleep -Milliseconds 500
    }
    return $texts
}
function DragFile($path) {
    # Fenêtre de l'Explorateur sur le dossier du fichier (moitié gauche), élément glissé à la souris vers la vue 3D de TSA
    $folder = Split-Path $path -Parent; $leaf = Split-Path $path -Leaf
    $before = @(Get-Process explorer -ErrorAction SilentlyContinue | ForEach-Object { $_.Id })
    Start-Process explorer.exe -ArgumentList ('"' + $folder + '"')
    $ew = $null; $deadline = (Get-Date).AddSeconds(20)
    $leafFolder = Split-Path $folder -Leaf
    while (-not $ew -and (Get-Date) -lt $deadline) {
        foreach ($top in $AE::RootElement.FindAll($TS::Children, [System.Windows.Automation.Condition]::TrueCondition)) {
            if ($top.Current.ClassName -eq "CabinetWClass" -and $top.Current.Name -like "*$leafFolder*") { $ew = $top; break }
        }
        Start-Sleep -Milliseconds 500
    }
    if (-not $ew) { Write-Output "  Explorateur introuvable pour $folder"; return $false }
    $eh = [IntPtr]$ew.Current.NativeWindowHandle
    $wa = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
    [Win]::ShowWindow($eh, 9) | Out-Null
    [Win]::MoveWindow($eh, $wa.Left, $wa.Top, [int]($wa.Width * 0.38), $wa.Height, $true) | Out-Null
    Start-Sleep -Seconds 2
    $item = $null; $deadline = (Get-Date).AddSeconds(15)
    while (-not $item -and (Get-Date) -lt $deadline) {
        foreach ($li in (Find-All (Fresh $eh) $CT::ListItem)) {
            $n = $li.Current.Name
            if ($n -eq $leaf -or $n -eq [System.IO.Path]::GetFileNameWithoutExtension($leaf)) { $item = $li; break }
        }
        Start-Sleep -Milliseconds 500
    }
    if (-not $item) { Write-Output "  élément $leaf introuvable dans l'Explorateur"; [MouseInput]::PostMessage($eh, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; return $false }
    $r = $item.Current.BoundingRectangle
    $sx = [int]($r.Left + [Math]::Min(40, $r.Width / 2)); $sy = [int]($r.Top + $r.Height / 2)
    $tr = New-Object Win+RECT; [Win]::GetWindowRect($script:w, [ref]$tr) | Out-Null
    $tx = [int](($tr.Left + $tr.Right) / 2 + 120); $ty = [int](($tr.Top + $tr.Bottom) / 2 + 60)
    [MouseInput]::SetCursorPos($sx, $sy) | Out-Null; Start-Sleep -Milliseconds 300
    [MouseInput]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 300
    for ($i = 1; $i -le 30; $i++) {
        [MouseInput]::SetCursorPos([int]($sx + ($tx - $sx) * $i / 30), [int]($sy + ($ty - $sy) * $i / 30)) | Out-Null
        [MouseInput]::mouse_event(1, 0, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 40
    }
    Start-Sleep -Milliseconds 600
    [MouseInput]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Seconds 2
    [MouseInput]::PostMessage($eh, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    Start-Sleep -Seconds 6
    return $true
}

$script:p = Start-Process -FilePath $Exe -ArgumentList @("--lang=fr", ('"' + $A1 + '"')) -PassThru
$deadline = (Get-Date).AddSeconds(120)
while ((Get-Date) -lt $deadline) { $script:p.Refresh(); if ($script:p.MainWindowHandle -ne [IntPtr]::Zero -and $script:p.MainWindowTitle -like "TSA*") { break }; Start-Sleep -Milliseconds 500 }
$script:w = $script:p.MainWindowHandle
Start-Sleep -Seconds 8
$wa = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
[Win]::ShowWindow($script:w, 9) | Out-Null
[Win]::MoveWindow($script:w, [int]($wa.Left + $wa.Width * 0.38), $wa.Top, [int]($wa.Width * 0.62), $wa.Height, $true) | Out-Null
Start-Sleep -Seconds 3
Write-Output "Départ : $(TabState)"

# --- Glisser-déposer ---------------------------------------------------------------------------------------------
DragFile $A2 | Out-Null
WaitTabs 2 | Out-Null; Write-Output "  $(TabState)"
Check ((Tabs).Count -eq 2 -and (CurrentTab) -like "Reservoir.tsa*d2*") "dépôt d'un fichier de même nom (autre dossier) : nouvel onglet distinct « d2 », « d1 » conservé"
DragFile $B | Out-Null
WaitTabs 3 | Out-Null; Write-Output "  $(TabState)"
Check ((Tabs).Count -eq 3 -and (CurrentTab) -like "Dalle.tsa*") "dépôt d'un troisième projet : nouvel onglet, les deux autres conservés"
Shot "d01_trois_onglets.png"
DragFile $A1 | Out-Null
Write-Output "  $(TabState)"
Check ((Tabs).Count -eq 3 -and (CurrentTab) -like "Reservoir.tsa*d1*") "dépôt d'un fichier déjà ouvert : son onglet est activé, pas de second exemplaire"
DragFile (Join-Path $Work "Corrompu.tsa") | Out-Null
$msgs = DismissDialogs 10
Write-Output ("  boîtes : " + ($msgs -join " || ")); Write-Output "  $(TabState)"
Check ($msgs.Count -gt 0 -and (Tabs).Count -eq 3 -and (CurrentTab) -like "Reservoir.tsa*d1*") "dépôt d'un fichier corrompu : erreur signalée, onglet vide retiré, onglets et onglet actif conservés"
$lock = [System.IO.File]::Open((Join-Path $Work "Verrouille.tsa"), 'Open', 'ReadWrite', 'None')
DragFile (Join-Path $Work "Verrouille.tsa") | Out-Null
$msgs2 = DismissDialogs 10
$lock.Close()
Write-Output ("  boîtes : " + ($msgs2 -join " || ")); Write-Output "  $(TabState)"
Check ($msgs2.Count -gt 0 -and (Tabs).Count -eq 3) "dépôt d'un fichier verrouillé par un autre processus : erreur signalée, onglets conservés"
DragFile (Join-Path $Work "Notes.txt") | Out-Null
$msgs3 = DismissDialogs 3
Write-Output "  $(TabState)"
Check ($msgs3.Count -eq 0 -and (Tabs).Count -eq 3) "dépôt d'un fichier non .tsa : refusé sans effet"

# --- Ctrl+Entrée après changement d'onglet -------------------------------------------------------------------------
[Win]::MoveWindow($script:w, $wa.Left, $wa.Top, $wa.Width, $wa.Height, $true) | Out-Null
Start-Sleep -Seconds 2
$origin = CurrentTab
Keys @("B")
Check ((BeamToolOn) -eq $true) "« Dessiner Poutre » (B) lancé dans « $origin »"
Keys @("Esc"); Keys @("Esc")
Check ((BeamToolOn) -eq $false) "outil annulé (Échap) dans « $origin »"
CloseToolPanels
Check ((ToolPanels).Count -eq 0) "panneau de l'outil fermé dans « $origin »"
Keys @("Ctrl", "Tab")
$second = CurrentTab
Keys @("Ctrl", "Enter")
Check ((BeamToolOn) -eq $true) "Ctrl+Entrée après changement d'onglet : outil activé dans l'onglet visible « $second »"
Shot "d02_ctrl_entree_onglet_visible.png"
# Changement d'onglet pendant que l'outil est actif : son panneau flottant ne doit pas rester sur l'autre document
Keys @("Ctrl", "Shift", "Tab")
$panels = ToolPanels
Shot "d03_outil_actif_puis_changement_onglet.png"
Check ($panels.Count -eq 0) "outil actif puis changement d'onglet : panneau de l'outil non visible sur l'autre document ($($panels.Count))"
Keys @("Ctrl", "Tab")
Check ((ToolPanels).Count -eq 1 -and (BeamToolOn) -eq $true) "retour sur « $second » : son panneau et son outil sont restitués"
Keys @("Esc"); Keys @("Esc")
CloseToolPanels
Keys @("Ctrl", "Shift", "Tab")
Check ((CurrentTab) -eq $origin -and (BeamToolOn) -eq $false) "retour sur « $origin » : l'outil n'y a pas été activé"
Keys @("Ctrl", "Tab"); Keys @("Ctrl", "Tab")
$third = CurrentTab
Keys @("Ctrl", "Enter")
Check ((BeamToolOn) -eq $true) "plusieurs changements d'onglet : outil activé dans « $third »"
Keys @("Esc"); Keys @("Esc")
# Commande lancée dans un onglet, puis onglet fermé (Ctrl+W) : Ctrl+Entrée agit sur l'onglet devenu visible
Keys @("B"); Keys @("Esc"); Keys @("Esc")
$closing = CurrentTab
Keys @("Ctrl", "W"); Start-Sleep -Seconds 2
DismissDialogs 2 | Out-Null
$after = CurrentTab
Write-Output "  après fermeture de « $closing » : $(TabState)"
Keys @("Ctrl", "Enter")
Check ((Tabs).Count -eq 2 -and (BeamToolOn) -eq $true) "onglet d'origine fermé : Ctrl+Entrée active l'outil dans « $after », sans erreur"
Keys @("Esc"); Keys @("Esc")

$closed = Close-App $script:p $script:w 60 $false
Check $closed "fermeture de l'application"
Write-Output "BILAN : $script:ok OK, $script:ko ÉCHEC"
