# =============================================================================
# scripts/detect_qt.ps1
# Détection automatique de Qt 6 et vérification stricte de compatibilité toolchain
# =============================================================================

[CmdletBinding()]
param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("MSVC", "MinGW", "None")]
    [string]$TargetCompiler = "MSVC",

    [string]$PreferredQtDir = ""
)

$Result = [PSCustomObject]@{
    QtFound      = $false
    QtVersion    = "NOT FOUND"
    QtToolchain  = "NOT FOUND"
    QtRootDir    = ""
    Qt6_DIR      = ""
    IsCompatible = $false
    ErrorMessage = ""
}

# -----------------------------------------------------------------------------
# Fonction pour analyser un dossier Qt
# -----------------------------------------------------------------------------
function Analyze-QtDir {
    param([string]$Path)
    if (-not $Path -or -not (Test-Path $Path)) { return $null }

    $cleanPath = $Path.Replace("/", "\").TrimEnd("\")
    if ($cleanPath -match 'lib\\cmake\\Qt6$') {
        $root = Split-Path (Split-Path (Split-Path $cleanPath -Parent) -Parent) -Parent
    } else {
        $root = $cleanPath
    }

    $qmake = Join-Path $root "bin\qmake.exe"
    $cmakeConfig = Join-Path $root "lib\cmake\Qt6\Qt6Config.cmake"

    if (-not (Test-Path $cmakeConfig)) {
        return $null
    }

    $toolchain = "Unknown"
    if ($root -match 'msvc') {
        $toolchain = "MSVC"
    } elseif ($root -match 'mingw') {
        $toolchain = "MinGW"
    }

    $version = "NOT FOUND"
    if (Test-Path $qmake) {
        try {
            $qmakeOut = & $qmake -query QT_VERSION 2>&1
            if ($qmakeOut -match '^\d+\.\d+\.\d+') {
                $version = $Matches[0]
            }
        } catch {}
    }

    if ($version -eq "NOT FOUND" -and $root -match 'Qt[\\/](\d+\.\d+\.\d+)') {
        $version = $Matches[1]
    }

    return [PSCustomObject]@{
        QtRootDir   = $root
        Qt6_DIR     = Join-Path $root "lib\cmake\Qt6"
        QtToolchain = $toolchain
        QtVersion   = $version
    }
}

# -----------------------------------------------------------------------------
# 1. Si un dossier explicite est fourni, le vérifier strictement
# -----------------------------------------------------------------------------
if ($PreferredQtDir) {
    $explicitAnalysis = Analyze-QtDir -Path $PreferredQtDir
    if ($explicitAnalysis) {
        $Result.QtFound = $true
        $Result.QtVersion = $explicitAnalysis.QtVersion
        $Result.QtToolchain = $explicitAnalysis.QtToolchain
        $Result.QtRootDir = $explicitAnalysis.QtRootDir
        $Result.Qt6_DIR = $explicitAnalysis.Qt6_DIR
        $Result.IsCompatible = ($explicitAnalysis.QtToolchain -eq $TargetCompiler)

        if (-not $Result.IsCompatible -and $TargetCompiler -ne "None") {
            $Result.ErrorMessage = "Erreur : la version de Qt détectée n'est pas compatible avec le compilateur sélectionné."
        }
        return $Result
    }
}

# -----------------------------------------------------------------------------
# 2. Recherche automatique de toutes les versions Qt disponibles
# -----------------------------------------------------------------------------
$candidateDirs = @()

if ($env:Qt6_DIR -and (Test-Path $env:Qt6_DIR)) { $candidateDirs += $env:Qt6_DIR }
if ($env:QTDIR -and (Test-Path $env:QTDIR)) { $candidateDirs += $env:QTDIR }

# Lecture de CMakePresets.json
$presetsFile = Join-Path $PSScriptRoot "..\CMakePresets.json"
if (Test-Path $presetsFile) {
    try {
        $presetJson = Get-Content $presetsFile -Raw | ConvertFrom-Json
        foreach ($cp in $presetJson.configurePresets) {
            if ($cp.cacheVariables.Qt6_DIR -and (Test-Path $cp.cacheVariables.Qt6_DIR)) {
                $candidateDirs += $cp.cacheVariables.Qt6_DIR
            }
        }
    } catch {}
}

# Scan de C:\Qt
if (Test-Path "C:\Qt") {
    $qtVersions = Get-ChildItem -Directory "C:\Qt" | Where-Object { $_.Name -match '^(5|6)\.' } | Sort-Object Name -Descending
    foreach ($v in $qtVersions) {
        $flavors = Get-ChildItem -Directory $v.FullName | Where-Object { $_.Name -match '(msvc|mingw)' }
        foreach ($fl in $flavors) {
            $candidateDirs += $fl.FullName
        }
    }
}

$detectedList = @()
foreach ($cand in $candidateDirs) {
    $analysis = Analyze-QtDir -Path $cand
    if ($analysis) {
        $alreadyIn = $detectedList | Where-Object { $_.QtRootDir -eq $analysis.QtRootDir }
        if (-not $alreadyIn) {
            $detectedList += $analysis
        }
    }
}

if ($detectedList.Count -eq 0) {
    $Result.ErrorMessage = "Aucune installation de Qt 6 valide n'a été trouvée sur le système."
    return $Result
}

# Priorité à la toolchain compatible
$compatibleQt = $detectedList | Where-Object { $_.QtToolchain -eq $TargetCompiler } | Select-Object -First 1

if ($compatibleQt) {
    $Result.QtFound = $true
    $Result.QtVersion = $compatibleQt.QtVersion
    $Result.QtToolchain = $compatibleQt.QtToolchain
    $Result.QtRootDir = $compatibleQt.QtRootDir
    $Result.Qt6_DIR = $compatibleQt.Qt6_DIR
    $Result.IsCompatible = $true
    return $Result
}

# Si aucune version compatible n'a été trouvée
$firstQt = $detectedList[0]
$Result.QtFound = $true
$Result.QtVersion = $firstQt.QtVersion
$Result.QtToolchain = $firstQt.QtToolchain
$Result.QtRootDir = $firstQt.QtRootDir
$Result.Qt6_DIR = $firstQt.Qt6_DIR
$Result.IsCompatible = ($firstQt.QtToolchain -eq $TargetCompiler)

if (-not $Result.IsCompatible -and $TargetCompiler -ne "None") {
    $Result.ErrorMessage = "Erreur : la version de Qt détectée n'est pas compatible avec le compilateur sélectionné."
}

return $Result
