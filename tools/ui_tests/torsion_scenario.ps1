param(
    [string]$Exe = "",
    [Parameter(Mandatory = $true)][string]$Project,
    [string]$OutDir = (Join-Path $env:TEMP "tsa_ui_tests")
)
if (-not $Exe) { $Exe = Join-Path $PSScriptRoot "..\..\build-ninja-debug\TSA.exe" }
$ErrorActionPreference = "Continue"
. "$PSScriptRoot\uia_lib.ps1"
$exe = $Exe
New-Item -ItemType Directory -Force $OutDir | Out-Null
$dir = $OutDir
$proj = $Project
$results = New-Object System.Collections.ArrayList
function Check($ok, $label) { $line = ($(if ($ok) { "OK    " } else { "ÉCHEC " }) + $label); [void]$results.Add($line); Write-Output $line }

function Get-Ctl($w, $name, $type, $seconds = 15) {
    $deadline = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $deadline) { $e = Find-ByName (Fresh $w) $name $type; if ($e) { return $e }; Start-Sleep -Milliseconds 500 }
    return $null
}
function Scale-Label($w) { $e = Find-NameLike (Fresh $w) "Échelle effective*" $CT::Text; if ($e) { return $e.Current.Name } else { return "" } }
function Dismiss($procId, $pattern, $buttonPatterns, $seconds = 5) {
    $d = Find-Dialog $procId $pattern $seconds
    if (-not $d) { return $null }
    $text = (Find-All $d $CT::Text | ForEach-Object { $_.Current.Name }) -join " / "
    foreach ($bp in $buttonPatterns) { $b = Find-NameLike $d $bp $CT::Button; if ($b) { Click $b; break } }
    return "[" + $d.Current.Name + "] " + $text
}
function Run-Analysis($w, $procId, $tag) {
    # F5 réel ; si aucune boîte de fin n'apparaît (touche perdue : fenêtre pas au premier plan), une
    # seconde tentative. Seule la boîte « Calcul terminé » prouve que le calcul a eu lieu.
    for ($attempt = 1; $attempt -le 2; $attempt++) {
        Focus-Window $w
        Press "{F5}"
        $pre = Dismiss $procId "Avertissements avant calcul*" @("*Oui*", "*Yes*") 4
        if ($pre) { Write-Host ("  avant calcul : " + $pre) }
        $deadline = (Get-Date).AddSeconds(60)
        while ((Get-Date) -lt $deadline) {
            $done = Dismiss $procId "Calcul termin*" @("OK", "*OK*") 2
            if ($done) { Write-Host ("  fin de calcul ($tag, tentative $attempt) : " + $done); return $true }
            $err = Dismiss $procId "Erreur*" @("OK", "*OK*") 1
            if ($err) { Write-Host ("  ERREUR : " + $err); return $false }
            if ($attempt -eq 1 -and (Get-Date) -gt $deadline.AddSeconds(-40)) { break }
        }
    }
    return $false
}

# --- 1. Ouverture d'un projet existant ------------------------------------------------------------
$p = Start-Process -FilePath $exe -ArgumentList ('"' + $proj + '"') -PassThru
$we = Wait-Window "TSA*torsion_ui*" 90
Check ($we -ne $null) "1. projet existant ouvert dans TSA.exe ($($we.Current.Name))"
$w = [IntPtr]$we.Current.NativeWindowHandle
Start-Sleep -Seconds 4
Focus-Window $w
Focus-Window $w; Start-Sleep -Seconds 2
$chk = Get-Ctl $w "Rotation des sections (torsion)" $CT::CheckBox 30
Check ($chk -ne $null) "1. case « Rotation des sections (torsion) » présente"
Check ((Toggle-State $chk) -eq "On") "1. case cochée par défaut"

# --- 2. Calcul (F5) --------------------------------------------------------------------------------
$ok = Run-Analysis $w $p.Id "TORSION"
Check $ok "2. calcul F5 terminé (cas TORSION)"
Start-Sleep -Seconds 2
$lbl = Scale-Label $w
Check ($lbl -match "×\d") "2. étiquette du facteur après calcul : '$lbl'"
$autoScale = $lbl
Capture $w "$dir\01_calcul_torsion.png" | Out-Null

# --- 3. Case de torsion : désactivation / réactivation --------------------------------------------
$chk = Get-Ctl $w "Rotation des sections (torsion)" $CT::CheckBox
Do-Toggle $chk
Start-Sleep -Seconds 1
$lblOff = Scale-Label $w
$chk = Get-Ctl $w "Rotation des sections (torsion)" $CT::CheckBox
Check ((Toggle-State $chk) -eq "Off") "3. case décochée (clic réel par UI Automation)"
Check ($lblOff -ne $autoScale) "3. facteur automatique recalculé sans rotation des sections : '$lblOff' (avant '$autoScale')"
Capture $w "$dir\02_sans_rotation.png" | Out-Null
Do-Toggle $chk
Start-Sleep -Seconds 1
$chk = Get-Ctl $w "Rotation des sections (torsion)" $CT::CheckBox
Check ((Toggle-State $chk) -eq "On" -and (Scale-Label $w) -eq $autoScale) "3. case recochée, facteur rétabli : '$(Scale-Label $w)'"
Capture $w "$dir\03_avec_rotation.png" | Out-Null

# --- 4. Facteur d'amplification ----------------------------------------------------------------------
$combo = Find-Combo $w "Auto (Adapt*"
Check ($combo -ne $null) "4. liste « Préréglage » trouvée (valeur '$(Value-Of $combo)')"
$sel = Select-ComboItem $w "Auto (Adapt*" "×100"
Start-Sleep -Seconds 1
Check ($sel -and (Scale-Label $w) -eq "Échelle effective : ×100") "4. préréglage ×100 (liste dépliée, élément activé) → '$(Scale-Label $w)'"
Capture $w "$dir\04_x100.png" | Out-Null
$sel = Select-ComboItem $w "×100" "×1 (Vraie*"
Start-Sleep -Seconds 1
Check ($sel -and (Scale-Label $w) -like "Échelle effective : ×1 (non amplifiée)") "4. préréglage ×1 → '$(Scale-Label $w)'"
Capture $w "$dir\05_x1.png" | Out-Null
$sel = Select-ComboItem $w "×1 (Vraie*" "Personnalis*"
Start-Sleep -Seconds 1
$spin = $null
foreach ($s in (Find-All (Fresh $w) $CT::Spinner)) { if ($s.Current.IsEnabled -and (Value-Of $s) -match "^\d") { $spin = $s } }
Check ($sel -and $spin -ne $null) "4. mode personnalisé, champ « Échelle manuelle » actif (valeur '$(Value-Of $spin)')"
if ($spin) {
    $set = $false
    try { $spin.GetCurrentPattern([System.Windows.Automation.RangeValuePattern]::Pattern).SetValue(250.0); $set = $true } catch {}
    if (-not $set) { try { Set-Value $spin "250,00"; $set = $true } catch {} }
    Start-Sleep -Seconds 1
    $l = Scale-Label $w
    Check ($l -eq "Échelle effective : ×250") "4. échelle manuelle 250 (valeur du compteur) → '$l'"
    Capture $w "$dir\06_x250.png" | Out-Null
}
$sel = Select-ComboItem $w "Personnalis*" "Auto (Adapt*"
Start-Sleep -Seconds 1
Check ($sel -and (Scale-Label $w) -eq $autoScale) "4. retour en automatique → '$(Scale-Label $w)'"

# --- 5. Changement de cas de charge (Paramètres d'analyse, Ctrl+F5) ---------------------------------
Focus-Window $w
Press "^{F5}"
$dlge = Find-Dialog $p.Id "*nalyse*" 10
Check ($dlge -ne $null) "5. boîte « Paramètres d'analyse » ouverte ($($dlge.Current.Name))"
$dlg = $null
if ($dlge) { $dlg = [IntPtr]$dlge.Current.NativeWindowHandle }
if ($dlg) {
    Start-Sleep -Seconds 1
    $loadCombo = Find-Combo $dlg "*TORSION*"
    Check ($loadCombo -ne $null) "5. liste des cas trouvée (valeur '$(Value-Of $loadCombo)')"
    $sel = Select-ComboItem $dlg "*TORSION*" "*FLEXION*"
    Check $sel "5. cas FLEXION sélectionné au clavier ('$(Value-Of (Find-Combo $dlg "*FLEXION*"))')"
    $save = Find-NameLike (Fresh $dlg) "Enregistrer et fermer" $CT::Button
    Click $save
    Start-Sleep -Seconds 1
    $ok = Run-Analysis $w $p.Id "FLEXION"
    Check $ok "5. calcul du cas FLEXION terminé"
    Start-Sleep -Seconds 2
    $lblF = Scale-Label $w
    Check ($lblF -match "×\d" -and $lblF -ne $autoScale) "5. facteur automatique du cas FLEXION : '$lblF' (TORSION : '$autoScale')"
    Capture $w "$dir\07_cas_flexion.png" | Out-Null
}

# --- 6. Redimensionnement ---------------------------------------------------------------------------
$h = $w
[Win]::ShowWindow($h, 9) | Out-Null
[Win]::MoveWindow($h, 60, 40, 1150, 760, $true) | Out-Null
Start-Sleep -Seconds 2
Check ((Scale-Label $w) -eq $lblF) "6. fenêtre 1150×760 : vue et panneau conservés ('$(Scale-Label $w)')"
Capture $w "$dir\08_redimensionnee.png" | Out-Null
[Win]::ShowWindow($h, 3) | Out-Null
Start-Sleep -Seconds 2
Capture $w "$dir\09_maximisee.png" | Out-Null
Check (-not $p.HasExited) "6. aucune fermeture inattendue après redimensionnement"

# --- 7. Enregistrement, fermeture, réouverture --------------------------------------------------------
Focus-Window $w
Press "^s"
Start-Sleep -Seconds 2
$wp = (Fresh $w).GetCurrentPattern([System.Windows.Automation.WindowPattern]::Pattern)
$wp.Close()
Start-Sleep -Seconds 2
$q = Dismiss $p.Id "*" @("*Ne pas enregistrer*", "*Non*", "*No*", "*Discard*") 3
if ($q) { Write-Output ("  fermeture : " + $q) }
$p.WaitForExit(40000) | Out-Null
Check ($p.HasExited) "7. TSA fermé (code $($p.ExitCode))"
$p2 = Start-Process -FilePath $exe -ArgumentList ('"' + $proj + '"') -PassThru
$w2e = Wait-Window "TSA*torsion_ui*" 90
$w2 = [IntPtr]$w2e.Current.NativeWindowHandle
Start-Sleep -Seconds 5
Focus-Window $w2
$chk2 = Get-Ctl $w2 "Rotation des sections (torsion)" $CT::CheckBox
Write-Output ("  réouverture : étiquette '" + (Scale-Label $w2) + "'")
Capture $w2 "$dir\10_reouverture.png" | Out-Null
$ok = Run-Analysis $w2 $p2.Id "FLEXION après réouverture"
Check $ok "7. calcul après réouverture (réglages d'analyse enregistrés : cas FLEXION)"
Start-Sleep -Seconds 2
Check ((Scale-Label $w2) -eq $lblF) "7. même facteur qu'avant fermeture : '$(Scale-Label $w2)'"
Capture $w2 "$dir\11_reouverture_calcul.png" | Out-Null
(Fresh $w2).GetCurrentPattern([System.Windows.Automation.WindowPattern]::Pattern).Close()
Start-Sleep -Seconds 2
$q = Dismiss $p2.Id "*" @("*Ne pas enregistrer*", "*Non*", "*No*", "*Discard*") 3
$p2.WaitForExit(40000) | Out-Null
Check ($p2.HasExited) "7. seconde session fermée (code $($p2.ExitCode))"

Write-Output "=== BILAN ==="
$results | ForEach-Object { Write-Output $_ }
