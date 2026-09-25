# =============================================================================
# scripts/detect_dependencies.ps1
# Détection et vérification de compatibilité de toolchain pour OpenCASCADE et VTK
# =============================================================================

[CmdletBinding()]
param(
    [ValidateSet("MSVC", "MinGW", "None")]
    [string]$TargetCompiler = "MSVC",

    [string]$SourceDir = ""
)

if (-not $SourceDir) {
    $SourceDir = Split-Path $PSScriptRoot -Parent
}
$sourcePath = (Resolve-Path $SourceDir).Path

$Result = [PSCustomObject]@{
    OCCTFound     = $false
    OCCTStatus    = "NOT FOUND"
    OCCTVersion   = "NOT FOUND"
    OCCTToolchain = "NOT FOUND"
    OCCTPath      = ""
    VTKFound      = $false
    VTKStatus     = "NOT FOUND"
    VTKVersion    = "NOT FOUND"
    VTKToolchain  = "NOT FOUND"
    VTKPath       = ""
    IsCompatible  = $true
    WarningMessage= ""
}

# -----------------------------------------------------------------------------
# 1. Détection d'OpenCASCADE (OCCT)
# -----------------------------------------------------------------------------
$occtDirs = Get-ChildItem -Directory -Path $sourcePath -Filter "opencascade*" -ErrorAction SilentlyContinue

if ($occtDirs -and $occtDirs.Count -gt 0) {
    $occtDir = $occtDirs[0].FullName
    $Result.OCCTFound = $true
    $Result.OCCTStatus = "FOUND"
    $Result.OCCTPath = $occtDir

    if ($occtDir -match 'opencascade-(\d+\.\d+\.\d+)') {
        $Result.OCCTVersion = $Matches[1]
    } else {
        $Result.OCCTVersion = "8.0.1"
    }

    if ($occtDir -match 'vc\d+' -or (Test-Path (Join-Path $occtDir "win64\vc14"))) {
        $Result.OCCTToolchain = "MSVC"
    } elseif ($occtDir -match 'mingw') {
        $Result.OCCTToolchain = "MinGW"
    } else {
        $Result.OCCTToolchain = "MSVC"
    }
} else {
    $Result.OCCTStatus = "NOT FOUND"
}

# -----------------------------------------------------------------------------
# 2. Détection de VTK dans 3rdparty
# -----------------------------------------------------------------------------
$tpDirs = Get-ChildItem -Directory -Path $sourcePath -Filter "3rdparty*" -ErrorAction SilentlyContinue
foreach ($tp in $tpDirs) {
    $vtkCandidate = Get-ChildItem -Directory -Path $tp.FullName -Filter "vtk*" -ErrorAction SilentlyContinue
    if ($vtkCandidate -and $vtkCandidate.Count -gt 0) {
        $vtkDir = $vtkCandidate[0].FullName
        $Result.VTKFound = $true
        $Result.VTKStatus = "FOUND"
        $Result.VTKPath = $vtkDir

        if ($vtkDir -match 'vtk-(\d+\.\d+\.\d+)') {
            $Result.VTKVersion = $Matches[1]
        } else {
            $Result.VTKVersion = "9.4.1"
        }

        if ($vtkDir -match 'vc\d+' -or $tp.Name -match 'vc\d+') {
            $Result.VTKToolchain = "MSVC"
        } else {
            $Result.VTKToolchain = "MinGW"
        }
        break
    }
}

# -----------------------------------------------------------------------------
# 3. Vérification de cohérence des toolchains
# -----------------------------------------------------------------------------
if ($TargetCompiler -ne "None") {
    if ($Result.OCCTFound -and $Result.OCCTToolchain -ne "NOT FOUND") {
        if ($Result.OCCTToolchain -ne $TargetCompiler) {
            $Result.IsCompatible = $false
            $Result.WarningMessage = "Incompatibilité toolchain : Compilateur = $TargetCompiler alors qu'OCCT = $($Result.OCCTToolchain). Les bibliothèques C++ doivent partager le même ABI."
        }
    }
}

return $Result
