param(
    [string]$Exe = "",
    [Parameter(Mandatory = $true)][string]$Project,
    [string]$OutDir = (Join-Path $env:TEMP "tsa_ui_tests")
)
if (-not $Exe) { $Exe = Join-Path $PSScriptRoot "..\..\build-ninja-debug\TSA.exe" }
# Essai réel de la boîte « Charge sur barre » : moment réparti (torsion) sur une poutre, cas refusé (axe
# « gravité »), moment réparti sur un treillis (accepté, signalé au calcul), affichage, calcul, enregistrement,
# réouverture. Projet attendu : torsion_dialog.tsa (TSA_TEST_DUMP_DIR, suite « torsion »).
$ErrorActionPreference = "Continue"
. "$PSScriptRoot\uia_lib.ps1"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$results = New-Object System.Collections.ArrayList
function Check($ok, $label) { $line = ($(if ($ok) { "OK    " } else { "ÉCHEC " }) + $label); [void]$results.Add($line); Write-Host $line }
function Get-Ctl($w, $name, $type, $seconds = 20) {
    $deadline = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $deadline) { $e = Find-ByName (Fresh $w) $name $type; if ($e) { return $e }; Start-Sleep -Milliseconds 500 }
    return $null
}
function Dismiss($procId, $pattern, $buttonPatterns, $seconds = 5) {
    $d = Find-Dialog $procId $pattern $seconds
    if (-not $d) { return $null }
    $text = (Find-All $d $CT::Text | ForEach-Object { $_.Current.Name }) -join " / "
    foreach ($bp in $buttonPatterns) { $b = Find-NameLike $d $bp $CT::Button; if ($b) { Click $b; break } }
    return "[" + $d.Current.Name + "] " + $text
}
function Spinners($w) {
    # Compteurs visibles dans l'ordre de la boîte : intensités (m1, m2) puis positions (x1, x2).
    $list = @()
    foreach ($e in (Find-All (Fresh $w) $CT::Spinner)) { if (-not $e.Current.IsOffscreen) { $list += $e } }
    return $list
}
function Find-Message($procId, $text, $seconds = 5) {
    # Boîte de message dont un texte contient exactement $text (casse comprise) ; renvoie [fenêtre, texte].
    $deadline = (Get-Date).AddSeconds($seconds)
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $procId)
    $winCond = New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $CT::Window)
    while ((Get-Date) -lt $deadline) {
        foreach ($top in $AE::RootElement.FindAll($TS::Children, $cond)) {
            $wins = @($top) + @($top.FindAll($TS::Descendants, $winCond))
            foreach ($win in $wins) {
                foreach ($t in (Find-All $win $CT::Text)) {
                    if ($t.Current.Name.Contains($text)) {
                        foreach ($b in (Find-All $win $CT::Button)) { if ($b.Current.Name -like "*OK*") { return @($win, $t.Current.Name, $b) } }
                    }
                }
            }
        }
        Start-Sleep -Milliseconds 300
    }
    return $null
}
function Status($dlg) { $e = Find-NameLike (Fresh $dlg) "Charge ML*" $CT::Text; if ($e) { return $e.Current.Name } else { return "" } }

$p = Start-Process -FilePath $Exe -ArgumentList @("--lang=fr", ('"' + $Project + '"')) -PassThru
$we = Wait-Window "TSA*torsion_dialog*" 90
Check ($we -ne $null) "1. projet ouvert ($($we.Current.Name))"
$w = [IntPtr]$we.Current.NativeWindowHandle
Start-Sleep -Seconds 4; Focus-Window $w; Start-Sleep -Seconds 2
[void](Get-Ctl $w "Rotation des sections (torsion)" $CT::CheckBox 30)

# --- Ouverture de la boîte « Charge sur barre » (raccourci Q, U ; sinon bouton du ruban) -----------------
# Bouton « Charge Uniforme… » du ruban (onglet Accueil), relu juste avant le clic ; sinon raccourci Q, U.
$dlge = $null
for ($k = 0; $k -lt 3 -and -not $dlge; $k++) {
    Focus-Window $w
    $btn = Find-NameLike (Fresh $w) "*Uniforme*" $CT::Button
    if ($btn) { Click-Mouse $btn }
    $dlge = Find-Dialog $p.Id "Charge sur barre" 6
}
if (-not $dlge) { Focus-Window $w; Press "q"; Press "u"; $dlge = Find-Dialog $p.Id "Charge sur barre" 6 }
Check ($dlge -ne $null) "2. boîte « Charge sur barre » ouverte"
if (-not $dlge) { Stop-Process -Id $p.Id -Force; exit 1 }
$dlg = [IntPtr]$dlge.Current.NativeWindowHandle
Start-Sleep -Seconds 1
Check ((Find-Combo $dlg "Poutre B1*") -ne $null) "2. barre cible : '$(Value-Of (Find-Combo $dlg "Poutre B1*"))'"

# --- Moment réparti sur la poutre ------------------------------------------------------------------------
$typeBefore = Value-Of (Find-Combo $dlg "Uniforme*")
$sel = Select-ComboItem $dlg "Uniforme*" "Moment réparti*"
Check $sel "3. type « Moment réparti / torsion » sélectionné ('$(Value-Of (Find-Combo $dlg "Moment réparti*"))', avant '$typeBefore')"
$dir = Find-Combo $dlg "Local x*"
Check ($dir -ne $null) "3. direction passée d'elle-même à « Local x » (torsion)"
$hint = Find-NameLike (Fresh $dlg) "Torsion*" $CT::Text
Check ($hint -ne $null) "3. aide affichée : '$(if ($hint) { $hint.Current.Name })'"
$sel = Select-ComboItem $dlg "G - *" "T*"
Check $sel "3. cas de charge « T » (cas calculé par le projet) : '$(Value-Of (Find-Combo $dlg "T*"))'"
$sp = Spinners $dlg
Check ($sp.Count -eq 4) "3. champs visibles : m1, m2, x1, x2 ($($sp.Count))"
for ($i = 0; $i -lt 2 -and $i -lt $sp.Count; $i++) { $sp[$i].GetCurrentPattern([System.Windows.Automation.RangeValuePattern]::Pattern).SetValue(2.0); Start-Sleep -Milliseconds 400 }
$sp = Spinners $dlg
$vals = ($sp | ForEach-Object { Value-Of $_ }) -join " ; "
Check ($sp.Count -eq 4 -and (Value-Of $sp[0]) -match "^2[,.]00" -and (Value-Of $sp[1]) -match "^2[,.]00") "3. m1 = m2 = 2 kNm/m, intervalle [x1 ; x2] : $vals"
Capture $dlg "$OutDir\d01_dialogue_moment.png" | Out-Null
Click-Mouse (Find-NameLike (Fresh $dlg) "Appliquer*" $CT::Button)
Start-Sleep -Seconds 1
$dup = Dismiss $p.Id "Charge déjà présente" @("*Non*", "*No*") 2
$err = Find-Message $p.Id "Barre #" 1
if ($err) { Write-Host ("  message inattendu : " + $err[1]); Click $err[2] }
$st1 = Status $dlg
Check ($st1 -like "Charge ML* appliquée.") "4. charge appliquée sur la poutre : '$st1'"

# --- Cas refusé : moment réparti sans axe (« Gravité ») ---------------------------------------------------
$sel = Select-ComboItem $dlg "Local x*" "Gravit*"
Click-Mouse (Find-NameLike (Fresh $dlg) "Appliquer*" $CT::Button)
$msg = Find-Message $p.Id "Choisir l'axe du moment réparti" 5
if ($msg) { Click $msg[2] }
Check ($sel -and $msg -ne $null) "5. axe « gravité » refusé par un message explicite : '$(if ($msg) { $msg[1] })'"
Check ((Status $dlg) -eq $st1) "5. aucune charge ajoutée par le refus (état : '$(Status $dlg)')"

# --- Treillis : accepté par la boîte, signalé au calcul ------------------------------------------------------
[void](Select-ComboItem $dlg "Gravit*" "Local x*")
$sel = Select-ComboItem $dlg "Poutre B1*" "Treillis T1*"
Check $sel "6. barre cible : treillis T1"
$sp = Spinners $dlg
for ($i = 0; $i -lt 2 -and $i -lt $sp.Count; $i++) { $sp[$i].GetCurrentPattern([System.Windows.Automation.RangeValuePattern]::Pattern).SetValue(1.0); Start-Sleep -Milliseconds 400 }
Click-Mouse (Find-NameLike (Fresh $dlg) "Appliquer*" $CT::Button)
Start-Sleep -Seconds 1
$st2 = Status $dlg
Check ($st2 -like "Charge ML* appliquée." -and $st2 -ne $st1) "6. moment réparti sur le treillis enregistré : '$st2'"
Click (Find-NameLike (Fresh $dlg) "Fermer" $CT::Button)
Start-Sleep -Seconds 1

# --- Affichage, calcul (avertissement attendu pour le treillis), enregistrement ------------------------------
Focus-Window $w
Press "f"; Start-Sleep -Seconds 1
Capture $w "$OutDir\d02_affichage_charges.png" | Out-Null
Focus-Window $w
Press "{F5}"
$pre = Dismiss $p.Id "Avertissements avant calcul*" @("*Oui*", "*Yes*") 8
Check ($pre -like "*barre articulée*") "7. avant calcul : moment réparti sur le treillis signalé non transmis"
Write-Host ("  " + $pre)
$done = $null
$deadline = (Get-Date).AddSeconds(60)
while (-not $done -and (Get-Date) -lt $deadline) { $done = Dismiss $p.Id "Calcul termin*" @("OK", "*OK*") 2 }
Check ($done -ne $null) "7. calcul terminé : $done"
Check ($done -like "*Torsion max : Mx = 6.00 kNm*") "7. synthèse de fin de calcul : torsion max affichée (Mx = m·L = 6 kNm)"
Start-Sleep -Seconds 2
$fitDef = Find-NameLike (Fresh $w) "*Déformée" $CT::Button
if ($fitDef) { Click $fitDef; Start-Sleep -Seconds 2 }
Capture $w "$OutDir\d03_resultats.png" | Out-Null
# Fermeture avec enregistrement par la boîte « Enregistrer les modifications » de TSA.
$closed = Close-App $p $w 40 $true
Check $closed "8. TSA fermé après « Enregistrer » dans la boîte de confirmation (code $($p.ExitCode))"
Check ((Get-Item $Project).LastWriteTime -gt (Get-Date).AddMinutes(-5)) "8. projet enregistré ($((Get-Item $Project).LastWriteTime.ToString('HH:mm:ss')))"

# --- Réouverture : charges restituées et affichées ------------------------------------------------------------
$p2 = Start-Process -FilePath $Exe -ArgumentList @("--lang=fr", ('"' + $Project + '"')) -PassThru
$w2e = Wait-Window "TSA*torsion_dialog*" 90
$w2 = [IntPtr]$w2e.Current.NativeWindowHandle
Start-Sleep -Seconds 5; Focus-Window $w2; Start-Sleep -Seconds 2
Press "f"; Start-Sleep -Seconds 1
Capture $w2 "$OutDir\d04_reouverture.png" | Out-Null
$stat = $null
for ($k = 0; $k -lt 40 -and -not $stat; $k++) { $stat = Find-NameLike (Fresh $w2) "Aucun résultat*" $CT::Text; if (-not $stat) { Start-Sleep -Milliseconds 500; if ($k % 8 -eq 7) { Focus-Window $w2 } } }
Check ($stat -ne $null) "9. réouverture : aucun résultat affiché tant que le calcul n'est pas relancé ('$(if ($stat) { $stat.Current.Name })')"
$closed2 = Close-App $p2 $w2
Check $closed2 "9. seconde session fermée (code $($p2.ExitCode))"
Write-Host "=== BILAN ==="
$results | ForEach-Object { Write-Host $_ }
