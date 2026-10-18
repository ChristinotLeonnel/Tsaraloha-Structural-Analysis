# Plan de travail et grille dans l'application réelle (UI Automation, vraie ligne de commande, vraie souris) sur une COPIE
# de examples/TSA_Demo_Batiment_R+5.tsa (ouverte en argument, jamais enregistrée) :
#   1. vue isométrique, plan annoncé dans la barre d'état (« Plan: XY (Z = … m) ») ;
#   2. WPMOVE 0 0 3 puis WPMOVE 2 0 0 : plan déplacé, type XY conservé (pas « Personnalisé »), origine annoncée ;
#   3. WPBACK deux fois : plan précédent rétabli à chaque fois ;
#   4. WPNEXT / WPPREV : plan parallèle suivant / précédent de la grille structurelle du projet ;
#   5. WPINEXT : origine sur l'intersection suivante, identifiant d'axes annoncé ;
#   6. glisser réel de la poignée Z du manipulateur (plan sélectionné par un clic sur son panneau, poignée repérée par sa
#      couleur dans une capture) : Échap pendant un glisser, plan rétabli ; puis glisser complet, plan monté ; WPBACK.
# Les captures (<Work>\shots) servent au contrôle visuel de la grille : une seule grille, dans le plan annoncé.
# Garde-fous (comme views_scenario.ps1) : aucun clic si la fenêtre n'est pas celle de la copie ; réglages
# HKCU\Software\TSA Engineering sauvegardés puis restaurés ; TSA fermé sans enregistrer.
param([string]$Exe = "", [Parameter(Mandatory = $true)][string]$Work, [switch]$SkipDrag)
$Root = Join-Path $PSScriptRoot "..\.."
if (-not $Exe) { $Exe = Join-Path $Root "build-ninja-release\TSA.exe" }
$ErrorActionPreference = "Continue"
. (Join-Path $PSScriptRoot "uia_lib.ps1")
Add-Type -AssemblyName System.Drawing
$OutDir = Join-Path $Work "shots"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$Log = Join-Path $Work "workplane_scenario.log"
"" | Set-Content $Log -Encoding UTF8
function Say($text) { Write-Host $text; Add-Content $Log $text -Encoding UTF8 }
$script:results = @()
function Check($label, [bool]$ok, $observed) {
    $script:results += [pscustomobject]@{ Step = $label; Ok = $ok }
    Say ("    [{0}] {1} — observé : {2}" -f $(if ($ok) { "OK" } else { "ÉCHEC" }), $label, $observed)
}

$RegBackup = Join-Path $Work "tsa_settings.reg"
& reg.exe export "HKCU\Software\TSA Engineering" $RegBackup /y *> $null
$script:projTitle = $null

function Park { $s = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds; [MouseInput]::SetCursorPos($s.Width - 2, $s.Height - 2) | Out-Null; Start-Sleep -Milliseconds 400 }
function Shot($name) { Park; Capture $script:w (Join-Path $OutDir $name) | Out-Null; Say "    capture : shots\$name" }
function Texts($pattern) {
    $out = @(); foreach ($t in (Find-All (Fresh $script:w) $CT::Text)) { $n = $t.Current.Name; if ($n -match $pattern) { $out += $n } }
    return ($out | Select-Object -Unique)
}
function RibbonButton($pattern) {
    foreach ($type in $CT::Button, $CT::CheckBox) {
        foreach ($b in (Find-All (Fresh $script:w) $type)) { if ($b.Current.Name -like $pattern -and -not $b.Current.IsOffscreen) { return $b } }
    }
    return $null
}
function RibbonTab($name) {
    foreach ($try in 1..3) {
        $t = Find-All (Fresh $script:w) $CT::TabItem | Where-Object { $_.Current.Name -eq $name } | Select-Object -First 1
        if (-not $t) { Start-Sleep -Milliseconds 500; continue }
        $sel = $null
        try { $sel = $t.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern) } catch {}
        if ($sel) { $sel.Select() } else { Click-Mouse $t }
        Start-Sleep -Milliseconds 800
        try { if ($sel -and $sel.Current.IsSelected) { return } } catch {}
    }
}
function PressRibbon($tab, $pattern) {
    Guard
    RibbonTab $tab
    $b = RibbonButton $pattern
    if (-not $b) { Say "ARRÊT : bouton « $pattern » introuvable (onglet $tab)"; Finish 2 }
    Click-Mouse $b; Start-Sleep -Seconds 2
}
function EnsureState($pattern, $state) {
    $b = RibbonButton $pattern
    if ($b) { try { if ((Toggle-State $b) -ne $state) { Click-Mouse $b } } catch {} ; Say "    $($b.Current.Name) : $(try { Toggle-State $b } catch { '?' })" }
}
function FocusMain { [Win]::SetForegroundWindow($script:w) | Out-Null; Start-Sleep -Milliseconds 300 }
function Finish([int]$code) {
    $closed = Close-App $script:p $script:w 60 $false
    $script:p.Refresh()
    if (-not $script:p.HasExited -and $script:projTitle -and ($script:p.MainWindowTitle -replace ' \*$', '') -eq $script:projTitle) {
        Say "    fermeture forcée de la copie d'essai (non enregistrée)"; Stop-Process -Id $script:p.Id -Force; $closed = $true
    }
    try { $script:p.WaitForExit(15000) | Out-Null } catch {}
    $ok = @($script:results | Where-Object { $_.Ok }).Count
    Say "fin : application fermée = $closed ; $ok / $($script:results.Count) vérifications réussies"
    if ($script:p.HasExited -and (Test-Path $RegBackup)) { & reg.exe delete "HKCU\Software\TSA Engineering" /f *> $null; & reg.exe import $RegBackup *> $null }
    elseif (-not $script:p.HasExited) { Say "    ATTENTION : TSA encore ouvert, réglages NON restaurés (sauvegarde : $RegBackup)" }
    exit $code
}
function Guard {
    $script:p.Refresh()
    if ($script:p.HasExited -or ($script:p.MainWindowTitle -replace ' \*$', '') -ne $script:projTitle) {
        Say "ARRÊT : fenêtre « $($script:p.MainWindowTitle) » au lieu de « $script:projTitle » — aucun clic envoyé"
        Finish 2
    }
}
function ViewRect {
    foreach ($e in (Find-All (Fresh $script:w) $CT::Pane) + (Find-All (Fresh $script:w) $CT::Custom) + (Find-All (Fresh $script:w) $CT::Group)) {
        if ($e.Current.ClassName -eq "OccView") { return $e.Current.BoundingRectangle }
    }
    return $null
}
function ConsoleText {
    foreach ($type in $CT::Document, $CT::Edit) {
        foreach ($e in (Find-All (Fresh $script:w) $type)) {
            try {
                $txt = $e.GetCurrentPattern([System.Windows.Automation.TextPattern]::Pattern).DocumentRange.GetText(-1)
                if ($txt -match "\[(INFO|WARN)\]") { return $txt }
            } catch {}
        }
    }
    return ""
}
function LastConsole($pattern) {
    FocusMain
    foreach ($try in 1..10) {
        $lines = @((ConsoleText) -split "`n" | Where-Object { $_ -match $pattern })
        if ($lines.Count) { return ($lines[-1] -replace '^(\s*\[[^\]]*\])+\s*', '').Trim() }   # « [hh:mm:ss] [INFO] [General] »
        Start-Sleep -Milliseconds 400
    }
    return ""
}
function CommandLine {
    return @(Find-All (Fresh $script:w) $CT::Edit | Where-Object { -not $_.Current.IsOffscreen -and $_.Current.BoundingRectangle.Width -gt 500 } |
             Sort-Object { $_.Current.BoundingRectangle.Top } -Descending)[0]
}
function EnsureConsole {
    # Console et ligne de commande : F2 (cmd.window.console) si le dock est masqué dans la disposition enregistrée
    if (CommandLine) { return }
    FocusMain; Press "{F2}"; Start-Sleep -Seconds 1
    if (-not (CommandLine)) { Say "ARRÊT : console introuvable après F2"; Finish 2 }
}
function Origin {
    # Origine courante du plan, lue dans la console : WPMOVE 0 0 0 l'annonce sans rien changer (historique inchangé)
    Command "WPMOVE 0 0 0"
    $m = LastConsole "Origine du plan de travail : "
    if ($m -match "\((-?[\d.]+) ; (-?[\d.]+) ; (-?[\d.]+)\) m") { return @([double]$Matches[1], [double]$Matches[2], [double]$Matches[3]) }
    return @([double]::NaN, [double]::NaN, [double]::NaN)
}
function Fmt($o) { return "({0} ; {1} ; {2})" -f $o[0], $o[1], $o[2] }
function Near($a, $b) { return ([math]::Abs($a[0] - $b[0]) -lt 0.0006 -and [math]::Abs($a[1] - $b[1]) -lt 0.0006 -and [math]::Abs($a[2] - $b[2]) -lt 0.0006) }
function Command($text) {
    Guard
    $e = CommandLine
    if (-not $e) { Say "ARRÊT : ligne de commande introuvable"; Finish 2 }
    Click-Mouse $e; Start-Sleep -Milliseconds 300
    Set-Value $e $text; Start-Sleep -Milliseconds 200
    Press "{ENTER}"; Start-Sleep -Seconds 2
}
function Launch($file) {
    $script:p = Start-Process -FilePath $Exe -ArgumentList "`"$file`"" -PassThru
    $deadline = (Get-Date).AddSeconds(120)
    $expected = "TSA — $(Split-Path $file -Leaf)"
    while ((Get-Date) -lt $deadline) {
        $script:p.Refresh()
        if ($script:p.MainWindowHandle -ne [IntPtr]::Zero -and ($script:p.MainWindowTitle -replace ' \*$', '') -eq $expected) { break }
        Start-Sleep -Milliseconds 500
    }
    $script:w = $script:p.MainWindowHandle
    Start-Sleep -Seconds 6
    $wa = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
    [Win]::ShowWindow($script:w, 9) | Out-Null
    [Win]::MoveWindow($script:w, $wa.Left, $wa.Top, $wa.Width, $wa.Height, $true) | Out-Null
    Start-Sleep -Seconds 2
    $script:p.Refresh()
    return (($script:p.MainWindowTitle -replace ' \*$', '') -eq $expected)
}

# --- 1. Copie du projet, vue isométrique ------------------------------------------------------------------------------
$source = Join-Path $Root "examples\TSA_Demo_Batiment_R+5.tsa"
$copy = Join-Path $Work "PlanTravail_R+5.tsa"
Copy-Item $source $copy -Force
Say "1. copie du projet de démonstration : $copy"
if (-not (Launch $copy)) { Say "ARRÊT : la copie d'essai n'est pas la fenêtre active (« $($script:p.MainWindowTitle) »)"; $script:projTitle = $null; Finish 2 }
$script:projTitle = "TSA — PlanTravail_R+5.tsa"
RibbonTab "Affichage"
EnsureState "Magnétisme Grille*" "Off"
EnsureState "Accrochage Objets*" "Off"
EnsureConsole
PressRibbon "Affichage" "Vue Isométrique*"
$o0 = Origin
Check "origine du plan de travail annoncée dans la console" (-not [double]::IsNaN($o0[2])) (Fmt $o0)
Shot "01_iso_plan_initial.png"

# --- 2. Saisie précise : WPMOVE --------------------------------------------------------------------------------------
Command "WPMOVE 0 0 3"
$o2 = Origin
Check "WPMOVE 0 0 3 : plan monté de 3 m" (Near $o2 @($o0[0], $o0[1], ($o0[2] + 3))) (Fmt $o2)
Shot "02_wpmove_z3.png"
Command "WPMOVE 2 0 0"
$o3 = Origin
Check "WPMOVE 2 0 0 : origine décalée de 2 m en X, même élévation" (Near $o3 @(($o0[0] + 2), $o0[1], ($o0[2] + 3))) (Fmt $o3)
Shot "03_wpmove_x2.png"

# --- 3. Plan précédent -------------------------------------------------------------------------------------------------
Command "WPBACK"
$b1 = LastConsole "Plan de travail précédent|Plan de travail inchangé"; $ob1 = Origin
Command "WPBACK"
$b2 = LastConsole "Plan de travail précédent|Plan de travail inchangé"; $ob2 = Origin
Check "WPBACK deux fois : origine à Z + 3 m, puis origine initiale" ($b1 -like "Plan de travail précédent*" -and (Near $ob1 $o2) -and $b2 -like "Plan de travail précédent*" -and (Near $ob2 $o0)) "$(Fmt $ob1) puis $(Fmt $ob2) ; console « $b2 »"
Shot "04_wpback.png"

# --- 4. Plans parallèles de la grille structurelle ---------------------------------------------------------------------
Command "WPNEXT"
$n1 = LastConsole "Plan de travail suivant|Plan de travail inchangé"; $on1 = Origin
Shot "05_wpnext.png"
Command "WPPREV"
$n2 = LastConsole "Plan de travail précédent :|Plan de travail inchangé"; $on2 = Origin
Check "WPNEXT puis WPPREV : plan parallèle suivant de la grille du projet, puis retour" ($n1 -like "Plan de travail suivant*" -and $on1[2] -gt $o0[2] + 0.01 -and (Near $on2 $o0)) "« $n1 » puis « $n2 » ; origine $(Fmt $on2)"

# --- 5. Intersections ---------------------------------------------------------------------------------------------------
Command "WPINEXT"
$i1 = LastConsole "Origine du plan sur l'intersection|Plan de travail inchangé"
Check "WPINEXT : origine sur l'intersection suivante, identifiant d'axes annoncé" ($i1 -like "Origine du plan sur l'intersection *") "« $i1 »"
Shot "06_wpinext.png"
Command "WPBACK"

if ($SkipDrag) { Finish 0 }

# --- 6. Glisser réel de la poignée Z du manipulateur -------------------------------------------------------------------
function BluePixels {
    # Pixels nettement bleus de la vue (clés « x,y » écran, pas de 2 px)
    Park
    $r = ViewRect
    $bmp = New-Object System.Drawing.Bitmap ([int]$r.Width), ([int]$r.Height)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen([int]$r.Left, [int]$r.Top, 0, 0, $bmp.Size)
    $set = New-Object 'System.Collections.Generic.HashSet[string]'
    for ($y = 0; $y -lt $bmp.Height; $y += 2) {
        for ($x = 0; $x -lt $bmp.Width; $x += 2) {
            $c = $bmp.GetPixel($x, $y)
            if ($c.B -gt 150 -and $c.B -gt $c.R + 90 -and $c.B -gt $c.G + 60) { $set.Add("$([int]$r.Left + $x),$([int]$r.Top + $y)") | Out-Null }
        }
    }
    $g.Dispose(); $bmp.Dispose()
    return ,$set
}
function BlueHandle($baseline) {
    # Tige de la flèche Z (bleue) du manipulateur : pixels bleus apparus depuis $baseline (le trièdre de vue en est
    # exclu), colonne la plus dense (l'anneau de rotation Z, bleu lui aussi, n'en donne que quelques-uns par colonne),
    # puis plus longue suite continue de pixels dans cette colonne : pied (origine du plan) et pointe de la flèche
    $now = BluePixels
    $pts = @(); foreach ($k in $now) { if (-not $baseline.Contains($k)) { $xy = $k.Split(','); $pts += ,@([int]$xy[0], [int]$xy[1]) } }
    if ($pts.Count -lt 20) { return $null }
    $col = $pts | Group-Object { $_[0] } | Sort-Object Count -Descending | Select-Object -First 1
    $xa = [int]$col.Name
    $ys = @($pts | Where-Object { [math]::Abs($_[0] - $xa) -le 2 } | ForEach-Object { $_[1] } | Sort-Object -Unique)
    $best = @($ys[0], $ys[0]); $runStart = $ys[0]
    for ($i = 1; $i -lt $ys.Count; $i++) {
        if ($ys[$i] - $ys[$i - 1] -gt 4) { $runStart = $ys[$i] }
        if ($ys[$i] - $runStart -gt $best[1] - $best[0]) { $best = @($runStart, $ys[$i]) }
    }
    return @{ Count = $pts.Count; Top = @($xa, $best[0]); Bottom = @($xa, $best[1]) }
}
function MoveTo($x, $y) {
    # Vrai mouvement souris (MOUSEEVENTF_MOVE | ABSOLUTE) : SetCursorPos seul n'atteint pas toujours la vue Qt
    $s = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    [MouseInput]::mouse_event(0x8001, [uint32]([math]::Round($x * 65535 / ($s.Width - 1))), [uint32]([math]::Round($y * 65535 / ($s.Height - 1))), 0, [UIntPtr]::Zero)
}
function Drag($from, $to, [switch]$EscapeBeforeRelease) {
    Guard
    MoveTo ($from[0] - 4) $from[1]; Start-Sleep -Milliseconds 300
    MoveTo $from[0] $from[1]; Start-Sleep -Milliseconds 700   # survol : la poignée devient active (détection)
    [MouseInput]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 300
    foreach ($k in 1..12) {
        MoveTo ([int]($from[0] + ($to[0] - $from[0]) * $k / 12)) ([int]($from[1] + ($to[1] - $from[1]) * $k / 12)); Start-Sleep -Milliseconds 80
    }
    Start-Sleep -Milliseconds 500
    if ($EscapeBeforeRelease) {
        Capture $script:w (Join-Path $OutDir "08a_pendant_glisser_avant_echap.png") | Out-Null   # souris immobile : preuve du déplacement
        Press "{ESC}"; Start-Sleep -Milliseconds 500
    }
    [MouseInput]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Seconds 1
}
# Point de saisie : 40 % de la tige depuis le pied (plus haut, l'anneau de rotation Z croise la flèche selon la vue)
function Grab($h) { return @([int]($h.Bottom[0] + ($h.Top[0] - $h.Bottom[0]) * 0.4), [int]($h.Bottom[1] + ($h.Top[1] - $h.Bottom[1]) * 0.4)) }

PressRibbon "Affichage" "Vue Isométrique*"
$od0 = Origin
$baseline = BluePixels
Say "6. pixels bleus avant sélection du plan : $($baseline.Count)"
# Panneau du plan : clic dans la vue, en bas à gauche (marge du panneau autour du bâtiment en vue isométrique)
$r = ViewRect
$h = $null
$k = 0
foreach ($f in @(@(0.36, 0.71), @(0.384, 0.752), @(0.346, 0.689), @(0.399, 0.773))) {
    $k++
    Guard
    $script:clickPt = @([int]($r.Left + $r.Width * $f[0]), [int]($r.Top + $r.Height * $f[1]))
    [MouseInput]::Click($script:clickPt[0], $script:clickPt[1]); Start-Sleep -Seconds 1
    $sel = LastConsole "sélectionné"
    Capture $script:w (Join-Path $OutDir "07_clic_$k.png") | Out-Null
    $h = BlueHandle $baseline
    Say "    clic à ($($f[0]) ; $($f[1])) : « $sel » ; pixels bleus : $(if ($h) { $h.Count } else { 0 })"
    if ($h) { break }
    $h = $null
}
Shot "07_plan_selectionne_gizmo.png"
if (-not $h) { Check "manipulateur affiché après sélection du plan" $false "aucune poignée bleue repérée"; Finish 1 }
$g1 = Grab $h
Say "    poignée Z : pied $($h.Bottom -join ',') ; pointe $($h.Top -join ',') ; saisie $($g1 -join ',')"
# a. Échap pendant le glisser (capture 08a prise avant Échap, souris immobile : plan et grille déplacés)
Drag $g1 @($g1[0], ($g1[1] - 100)) -EscapeBeforeRelease
Shot "08_apres_echap.png"
$odE = Origin
$hE = BlueHandle $baseline
if (-not $hE) {
    # Échap a aussi vidé la sélection (manipulateur détaché) : nouveau clic sur le panneau
    Guard; [MouseInput]::Click($script:clickPt[0], $script:clickPt[1]); Start-Sleep -Seconds 1
    $hE = BlueHandle $baseline
}
Check "Échap pendant le glisser : plan rétabli à son origine, gizmo à sa place" ((Near $odE $od0) -and $hE -and [math]::Abs($hE.Bottom[1] - $h.Bottom[1]) -le 4) "origine $(Fmt $odE) ; pied de la poignée $(if ($hE) { $hE.Bottom -join ',' } else { 'absent' })"
# b. Glisser réel de la poignée Z
Drag $g1 @($g1[0], ($g1[1] - 120))
Shot "09_apres_glisser.png"
$od1 = Origin
Check "glisser de la poignée Z : plan monté verticalement (X, Y inchangés)" ($od1[2] -gt $od0[2] + 0.05 -and [math]::Abs($od1[0] - $od0[0]) -lt 0.0006 -and [math]::Abs($od1[1] - $od0[1]) -lt 0.0006) "avant $(Fmt $od0) ; après $(Fmt $od1)"
Command "WPBACK"
$back = LastConsole "Plan de travail précédent|Plan de travail inchangé"
$od3 = Origin
Check "WPBACK après le glisser : origine d'avant le glisser" (Near $od3 $od0) "$(Fmt $od3) ; console « $back »"
Finish 0
