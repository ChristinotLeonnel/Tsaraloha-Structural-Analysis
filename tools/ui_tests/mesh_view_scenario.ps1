param(
    [string]$Exe = "",
    [Parameter(Mandatory = $true)][string]$Project,
    [string]$OutDir = (Join-Path $env:TEMP "tsa_ui_tests")
)
# Essai réel de l'affichage du maillage dans la vue 3D de TSA (docs/MESH_VISUALIZATION.md) : génération Gmsh
# depuis le menu, maillage affiché (comptes relus dans « Inspection du maillage »), modes d'affichage, maillage
# seul, étiquettes, sélection par clic dans la vue, redimensionnement, calcul et déplacements sur le maillage,
# enregistrement, réouverture, cycles ouverture / fermeture, journal de session.
if (-not $Exe) { $Exe = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "..\..\build-ninja-debug\TSA.exe" }
$ErrorActionPreference = "Continue"
. (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "uia_lib.ps1")
New-Item -ItemType Directory -Force $OutDir | Out-Null
$dir = $OutDir
$results = New-Object System.Collections.ArrayList
function Check($ok, $label) { $line = ($(if ($ok) { "OK    " } else { "ÉCHEC " }) + $label); [void]$results.Add($line); Write-Output $line }
$logDir = Join-Path (Split-Path -Parent $Exe) "logs\sessions"
if (-not (Test-Path $logDir)) { $logDir = Join-Path $env:LOCALAPPDATA "TSA Engineering\TSA\logs\sessions" }
$started = Get-Date

# Position (1…n, séparateurs exclus) des commandes du menu « Affichage du maillage » (populateMeshViewMenu) :
# Qt n'expose pas les éléments d'un menu déroulant du ruban à UI Automation, le menu est parcouru au clavier.
$MeshMenu = @("Filaire", "Faces et arêtes", "Faces seules", "Éléments réduits", "Arêtes du maillage", "Nœuds du maillage",
    "Faces transparentes", "Maillage seul (masquer la géométrie)", "Numéros des nœuds", "Numéros des éléments", "Taille des nœuds...",
    "Épaisseur des traits...", "Éléments 1D", "Éléments 2D", "Éléments 3D", "Faces intérieures des volumes", "Coupe du maillage...",
    "Isoler des groupes...", "Afficher tous les groupes", "Sélection dans le maillage", "Cadrer sur le maillage",
    "Déplacements sur le maillage", "Facteur de déformation...", "Animer la déformée")
function Wait-ProcessWindow($proc, $seconds = 90) {
    $deadline = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $deadline) {
        $proc.Refresh()
        if ($proc.MainWindowHandle -ne [IntPtr]::Zero -and $proc.MainWindowTitle -like "TSA*meshview_ui*") { return $AE::FromHandle($proc.MainWindowHandle) }
        Start-Sleep -Milliseconds 500
    }
    return $null
}
function Screen($path) {
    $bmp = New-Object System.Drawing.Bitmap ([System.Windows.Forms.Screen]::PrimaryScreen.Bounds.Width), ([System.Windows.Forms.Screen]::PrimaryScreen.Bounds.Height)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen(0, 0, 0, 0, $bmp.Size); $bmp.Save($path); $g.Dispose(); $bmp.Dispose()
}
function Dialogs-Of($procId) {
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $procId)
    $names = @()
    foreach ($top in $AE::RootElement.FindAll($TS::Children, $cond)) {
        foreach ($d in (Find-All $top $CT::Window)) { $names += $d.Current.Name }
    }
    return ($names -join " | ")
}
function Close-Or-Kill($proc, $w, $seconds, [bool]$save) {
    $ok = Close-App $proc $w $seconds $save
    if (-not $ok) {
        Write-Output ("  fenêtres ouvertes du processus : " + (Dialogs-Of $proc.Id))
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }
    return $ok
}
function Wait-Tree($w) {
    for ($i = 0; $i -lt 40; $i++) {
        if ((Fresh $w).FindAll($TS::Descendants, [System.Windows.Automation.Condition]::TrueCondition).Count -gt 60) { return $true }
        Start-Sleep -Seconds 1
    }
    return $false
}
function Find-Visible($w, $regex) {
    foreach ($e in (Fresh $w).FindAll($TS::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) {
        if ($e.Current.Name -match $regex -and -not $e.Current.IsOffscreen) { return $e }
    }
    return $null
}
function Analysis-Tab($w) {
    foreach ($e in (Find-All (Fresh $w) $CT::TabItem)) { if ($e.Current.Name -eq "Analyse") { Click-Mouse $e; return $true } }
    return $false
}
function Ribbon-Click($w, $regex) {
    Focus-Window $w
    Analysis-Tab $w | Out-Null
    $b = Find-Visible $w $regex
    if ($b) { Click-Mouse $b; return $true }
    return $false
}
function Mesh-View($w, $label) {
    $index = [Array]::IndexOf($MeshMenu, $label) + 1
    if ($index -le 0) { return $false }
    if (-not (Ribbon-Click $w "^Affichage du maillage$")) { return $false }
    Start-Sleep -Milliseconds 700
    for ($i = 0; $i -lt $index; $i++) { [System.Windows.Forms.SendKeys]::SendWait("{DOWN}"); Start-Sleep -Milliseconds 60 }
    Press "{ENTER}"
    Start-Sleep -Milliseconds 900
    return $true
}
function Inspect-Text($w) {
    foreach ($t in @($CT::Edit, $CT::Document)) {
        foreach ($e in (Find-All (Fresh $w) $t)) { if ($e.Current.Name -eq "Inspection du maillage") { return (Value-Of $e) } }
    }
    return ""
}
function Displayed($text) {
    if ($text -match "Affichés : (\d+) nœuds, (\d+) éléments \(1D : (\d+), 2D : (\d+), 3D : (\d+)\)") {
        return @([int]$Matches[1], [int]$Matches[2], [int]$Matches[3], [int]$Matches[4], [int]$Matches[5])
    }
    return $null
}
function View-Element($w) {
    $cond = New-Object System.Windows.Automation.PropertyCondition($AE::ClassNameProperty, "OccView")
    return (Fresh $w).FindFirst($TS::Descendants, $cond)
}
function Dismiss($procId, $pattern, $buttonPatterns, $seconds = 5) {
    $d = Find-Dialog $procId $pattern $seconds
    if (-not $d) { return $null }
    $text = (Find-All $d $CT::Text | ForEach-Object { $_.Current.Name }) -join " / "
    $title = $d.Current.Name
    # Clic sur le bouton, puis contrôle : la boîte doit avoir disparu (sinon nouvel essai, puis Entrée).
    for ($k = 0; $k -lt 4; $k++) {
        try {
            if ($k -lt 2) { foreach ($bp in $buttonPatterns) { $b = Find-NameLike $d $bp $CT::Button; if ($b) { Click-Mouse $b; break } } }
            else { [Win]::SetForegroundWindow([IntPtr]$d.Current.NativeWindowHandle) | Out-Null; Press "{ENTER}" }
        } catch {}
        Start-Sleep -Milliseconds 700
        try { $null = $d.Current.Name; $gone = $d.Current.IsOffscreen } catch { $gone = $true }
        if ($gone) { break }
    }
    return "[" + $title + "] " + $text
}
function Run-Analysis($w, $procId) {
    for ($attempt = 1; $attempt -le 2; $attempt++) {
        Focus-Window $w
        Press "{F5}"
        $deadline = (Get-Date).AddSeconds(90)
        while ((Get-Date) -lt $deadline) {
            $pre = Dismiss $procId "Avertissements avant calcul*" @("*Oui*", "*Yes*") 1
            if ($pre) { Write-Host ("  avant calcul : " + $pre) }
            $m = Dismiss $procId "*aillage*" @("*Oui*", "*Continuer*", "OK") 1
            if ($m) { Write-Host ("  maillage : " + $m) }
            $done = Dismiss $procId "Calcul termin*" @("OK", "*OK*") 1
            if ($done) { Write-Host ("  fin de calcul : " + $done); return $true }
            $err = Dismiss $procId "*Erreur*" @("OK", "*OK*") 1
            if ($err) { Write-Host ("  ERREUR : " + $err); return $false }
            if ($attempt -eq 1 -and (Get-Date) -gt $deadline.AddSeconds(-70)) { break }
        }
    }
    return $false
}

# --- 1. Ouverture du projet --------------------------------------------------------------------------
$p = Start-Process -FilePath $Exe -ArgumentList ('"' + $Project + '"') -PassThru
$we = Wait-ProcessWindow $p 90
Check ($we -ne $null) "1. projet ouvert dans TSA.exe ($($we.Current.Name))"
$w = [IntPtr]$we.Current.NativeWindowHandle
[Win]::ShowWindow($w, 3) | Out-Null
Check (Wait-Tree $w) "1. interface accessible (UI Automation)"
[Win]::ShowWindow($w, 3) | Out-Null
Start-Sleep -Seconds 2
Capture $w "$dir\01_projet.png" | Out-Null

# --- 2. Génération du maillage (Gmsh) par le menu -------------------------------------------------------
# Inspection ouverte avant la génération : « Aucun maillage affiché. », puis comptes du maillage généré.
Check (Mesh-View $w "Sélection dans le maillage") "2. ruban Analyse > Affichage du maillage > Sélection dans le maillage"
$before = Inspect-Text $w
Check ($before -match "Aucun maillage affiché") "2. inspection ouverte, aucun maillage avant la génération"
Check (Ribbon-Click $w "^Générer le\s+maillage$") "2. ruban Analyse > Générer le maillage (Gmsh)"
$text = ""
$deadline = (Get-Date).AddSeconds(180)
while (-not ($text -match "Maillage du projet") -and (Get-Date) -lt $deadline) { Start-Sleep -Seconds 1; $text = Inspect-Text $w }
Check ($text -match "Maillage du projet") "2. génération terminée : maillage transmis à la vue 3D"
Start-Sleep -Seconds 2
Capture $w "$dir\02_maillage_genere.png" | Out-Null

# --- 3. Maillage réel dans la vue 3D : comptes relus dans l'inspection -------------------------------------
$counts = Displayed $text
Check ($counts -ne $null -and $counts[2] -gt 0 -and $counts[3] -gt 0 -and $counts[4] -gt 0) ("3. maillage affiché dans la vue 3D : " + $(if ($counts) { "$($counts[0]) nœuds, $($counts[1]) éléments (1D $($counts[2]), 2D $($counts[3]), 3D $($counts[4]))" } else { "aucun compte lu" }))
Check ($text -match "Maillage du projet") "3. inspection : maillage du projet (pas l'aperçu)"
Check ($text -match "Arêtes uniques tracées : (\d+)" -and [int]$Matches[1] -gt 0) "3. arêtes uniques tracées : $($Matches[1])"
$initial = $counts
$text -split "`n" | Select-Object -First 8 | ForEach-Object { Write-Output ("    | " + $_.TrimEnd()) }
Check (Mesh-View $w "Cadrer sur le maillage") "3. cadrage sur le maillage"
Capture $w "$dir\03_maillage_vue3d.png" | Out-Null

# --- 4. Modes d'affichage ---------------------------------------------------------------------------------
$k = 4
foreach ($mode in @("Filaire", "Faces seules", "Éléments réduits", "Faces et arêtes")) {
    $ok = Mesh-View $w $mode
    Start-Sleep -Milliseconds 800
    $t = Inspect-Text $w
    $edges = if ($t -match "Arêtes uniques tracées : (\d+)") { [int]$Matches[1] } else { -1 }
    $expectEdges = ($mode -eq "Filaire" -or $mode -eq "Faces et arêtes")
    Check ($ok -and (($edges -gt 0) -eq $expectEdges)) "4. mode « $mode » (arêtes tracées : $edges)"
    Capture $w ("$dir\0{0}_mode{0}.png" -f $k) | Out-Null
    $k++
}
Check (Mesh-View $w "Maillage seul (masquer la géométrie)") "4. maillage seul (géométrie masquée)"
Start-Sleep -Seconds 1
Capture $w "$dir\10_maillage_seul.png" | Out-Null
Check (Mesh-View $w "Numéros des éléments") "4. numéros des éléments"
$t = Inspect-Text $w
Check ($t -match "Étiquettes : (\d+)") "4. étiquettes affichées : $($Matches[1])"
Capture $w "$dir\11_numeros.png" | Out-Null
Mesh-View $w "Numéros des éléments" | Out-Null
Check (Mesh-View $w "Éléments 3D") "4. éléments 3D masqués"
$t = Inspect-Text $w
$c = Displayed $t
Check ($c -ne $null -and $c[4] -eq 0 -and $c[3] -eq $initial[3]) "4. filtre de dimension appliqué (3D : $($c[4]), 2D : $($c[3]))"
Mesh-View $w "Éléments 3D" | Out-Null
Mesh-View $w "Maillage seul (masquer la géométrie)" | Out-Null

# --- 5. Sélection par clic dans la vue 3D -----------------------------------------------------------------
Mesh-View $w "Cadrer sur le maillage" | Out-Null
$view = View-Element $w
Check ($view -ne $null) "5. vue 3D (OccView) trouvée"
$picked = $false
$t = ""
if ($view) {
    $r = $view.Current.BoundingRectangle
    foreach ($f in @(@(0.5, 0.5), @(0.45, 0.45), @(0.55, 0.5), @(0.5, 0.6), @(0.4, 0.55), @(0.6, 0.4))) {
        [MouseInput]::Click([int]($r.Left + $r.Width * $f[0]), [int]($r.Top + $r.Height * $f[1])); Start-Sleep -Milliseconds 900
        $t = Inspect-Text $w
        if ($t -match "— Sélection —") { $picked = $true; break }
    }
}
$sel = if ($t -match "— Sélection —\s*\r?\n(.*)") { $Matches[1] } else { "" }
Check $picked "5. clic dans la vue : entité du maillage sélectionnée ($sel)"
Check ($t -match "Gmsh : (élément|nœud) \d+") "5. propriétés : étiquette Gmsh affichée"
($t -split "— Sélection —")[1] -split "`n" | Select-Object -First 8 | ForEach-Object { if ($_.Trim()) { Write-Output ("    | " + $_.TrimEnd()) } }
Capture $w "$dir\12_selection.png" | Out-Null

# --- 6. Redimensionnement -----------------------------------------------------------------------------------
[Win]::ShowWindow($w, 9) | Out-Null
[Win]::MoveWindow($w, 40, 40, 1100, 720, $true) | Out-Null
Start-Sleep -Seconds 2
Capture $w "$dir\13_redimensionne.png" | Out-Null
$t = Inspect-Text $w
Check ((Displayed $t) -ne $null) "6. fenêtre redimensionnée (1100 × 720) : maillage toujours affiché"
[Win]::ShowWindow($w, 3) | Out-Null
Start-Sleep -Seconds 2

# --- 7. Calcul et déplacements sur le maillage ---------------------------------------------------------------
$solved = Run-Analysis $w $p.Id
Check $solved "7. calcul F5 terminé"
if ($solved) {
    Check (Mesh-View $w "Déplacements sur le maillage") "7. déplacements sur le maillage"
    Start-Sleep -Seconds 2
    $t = Inspect-Text $w
    $applied = $t -match "Résultats : déplacements, facteur de déformation ([0-9.eE+-]+)"
    Check $applied ("7. champ de déplacements appliqué" + $(if ($applied) { " (facteur $($Matches[1]))" } else { "" }))
    if (-not $applied) { Screen "$dir\14_echec_ecran.png"; Write-Output ("  fenêtres : " + (Dialogs-Of $p.Id)); $t -split "`n" | Select-Object -First 12 | ForEach-Object { Write-Output ("    | " + $_.TrimEnd()) } }
    Capture $w "$dir\14_deplacements.png" | Out-Null
}

# --- 8. Enregistrement, réouverture, cycles -------------------------------------------------------------------
Check (Close-Or-Kill $p $w 60 $true) "8. fermeture avec enregistrement"
for ($cycle = 1; $cycle -le 3; $cycle++) {
    $p = Start-Process -FilePath $Exe -ArgumentList ('"' + $Project + '"') -PassThru
    $we = Wait-ProcessWindow $p 90
    $w = [IntPtr]$we.Current.NativeWindowHandle
    Wait-Tree $w | Out-Null
    Start-Sleep -Seconds 2
    if ($cycle -eq 1) {
        [Win]::ShowWindow($w, 3) | Out-Null
        Check (Ribbon-Click $w "^Afficher le maillage$") "8. réouverture : ruban « Afficher le maillage »"
        Mesh-View $w "Sélection dans le maillage" | Out-Null
        $t = Inspect-Text $w
        $c = Displayed $t
        Check ($c -ne $null -and $c[0] -eq $initial[0] -and $c[1] -eq $initial[1]) "8. maillage relu du projet et affiché à l'identique ($($c -join ' / '))"
        Capture $w "$dir\15_reouverture.png" | Out-Null
    }
    Check (Close-Or-Kill $p $w 40 $false) "8. cycle $cycle : ouverture / fermeture"
}

# --- 9. Journal de session -----------------------------------------------------------------------------------
$logs = @(Get-ChildItem $logDir -Filter "session_*.log" -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -ge $started })
$errors = @(); $meshLines = @()
foreach ($l in $logs) {
    $errors += @(Select-String -Path $l.FullName -Pattern "\[(ERROR|CRITICAL|FATAL) *\]" | ForEach-Object { $_.Line })
    $meshLines += @(Select-String -Path $l.FullName -Pattern "aillage" | ForEach-Object { $_.Line })
}
Check ($logs.Count -ge 4) "9. journaux de session : $($logs.Count)"
Check ($errors.Count -eq 0) "9. aucune erreur dans les journaux ($($errors.Count))"
$errors | Select-Object -First 10 | ForEach-Object { Write-Output ("  " + $_) }
$meshLines | Select-Object -First 12 | ForEach-Object { Write-Output ("  " + $_) }

$failed = @($results | Where-Object { $_ -like "ÉCHEC*" }).Count
Write-Output ""
Write-Output ("Bilan : " + ($results.Count - $failed) + " / " + $results.Count + " vérifications réussies")
