# =============================================================================
# scripts/setup_build.ps1
# Script principal d'orchestration, détection automatique du compilateur,
# vérification des compatibilités Qt / OCCT et configuration CMake pour TSA
# =============================================================================

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Release",

    [switch]$ForceMinGW,
    [switch]$Build,
    [string]$CustomBuildDir = "",
    [string]$PreferredQtDir = "",
    [string]$MinGWInstallPath = "C:\TSA\tools\mingw64"
)

$ErrorActionPreference = "Stop"
$SourceDir = Split-Path $PSScriptRoot -Parent

# -----------------------------------------------------------------------------
# 1. Vérification du système d'exploitation et de l'architecture
# -----------------------------------------------------------------------------
if ($PSVersionTable.PSEdition -ne "Core" -and [System.Environment]::OSVersion.Platform -ne 'Win32NT') {
    Write-Error "Ce script est exclusivement conçu pour les systèmes d'exploitation Windows."
    exit 1
}

$is64Bit = [Environment]::Is64BitOperatingSystem
$archString = if ($is64Bit) { "x64" } else { "x86" }
if (-not $is64Bit) {
    Write-Error "Erreur : TSA requiert une architecture Windows 64 bits (x64)."
    exit 1
}

# -----------------------------------------------------------------------------
# 2. Détection du compilateur (MSVC prioritaire, MinGW en repli)
# -----------------------------------------------------------------------------
$detectCompilerScript = Join-Path $PSScriptRoot "detect_compiler.ps1"
$installMinGWScript   = Join-Path $PSScriptRoot "install_mingw.ps1"
$detectQtScript       = Join-Path $PSScriptRoot "detect_qt.ps1"
$detectDepsScript     = Join-Path $PSScriptRoot "detect_dependencies.ps1"

$compilerInfo = & $detectCompilerScript -ForceMinGW:$ForceMinGW

# Si aucun compilateur n'a été trouvé, procéder à l'installation automatique de MinGW-w64
if (-not $compilerInfo.Available) {
    Write-Host "Aucun compilateur C++ fonctionnel trouvé." -ForegroundColor Yellow
    Write-Host "Téléchargement et installation automatique de MinGW-w64..." -ForegroundColor Cyan

    $installedBin = & $installMinGWScript -InstallDir $MinGWInstallPath -SkipCheckMSVC
    if (-not $installedBin -or -not (Test-Path $installedBin)) {
        Write-Error "Échec de l'installation automatique de MinGW-w64. Veuillez installer un compilateur C++."
        exit 1
    }

    # Re-détection après installation
    $compilerInfo = & $detectCompilerScript -ForceMinGW -PreferredMinGWPath $installedBin
    if (-not $compilerInfo.Available) {
        Write-Error "MinGW-w64 a été installé mais n'a pas pu être validé par test de compilation."
        exit 1
    }
}

$selectedCompiler = $compilerInfo.SelectedCompiler

# Si MinGW est sélectionné, s'assurer que son dossier bin est dans le PATH du processus
if ($selectedCompiler -eq "MinGW" -and $compilerInfo.MinGWBin) {
    $currentPaths = $env:PATH -split ';'
    if ($currentPaths -notcontains $compilerInfo.MinGWBin) {
        $env:PATH = "$($compilerInfo.MinGWBin);$env:PATH"
        Write-Host "Dossier MinGW bin ajouté au PATH de la session : $($compilerInfo.MinGWBin)" -ForegroundColor Cyan
    }
}

# -----------------------------------------------------------------------------
# 3. Détection et compatibilité de Qt 6 (Règle stricte)
# -----------------------------------------------------------------------------
$qtInfo = & $detectQtScript -TargetCompiler $selectedCompiler -PreferredQtDir $PreferredQtDir

if (-not $qtInfo.QtFound) {
    Write-Error $qtInfo.ErrorMessage
    exit 1
}

if (-not $qtInfo.IsCompatible) {
    Write-Host ""
    Write-Host "======================================================================" -ForegroundColor Red
    Write-Host $qtInfo.ErrorMessage -ForegroundColor Red
    Write-Host "Le compilateur $selectedCompiler ne peut pas lier des bibliothèques Qt $($qtInfo.QtToolchain)." -ForegroundColor Red
    Write-Host "Veuillez installer le paquet Qt 6 correspondant ou changer de compilateur." -ForegroundColor Red
    Write-Host "======================================================================" -ForegroundColor Red
    exit 1
}

# -----------------------------------------------------------------------------
# 4. Détection des dépendances externes (OpenCASCADE et VTK)
# -----------------------------------------------------------------------------
$depsInfo = & $detectDepsScript -TargetCompiler $selectedCompiler -SourceDir $SourceDir

if (-not $depsInfo.IsCompatible) {
    Write-Warning $depsInfo.WarningMessage
}

# -----------------------------------------------------------------------------
# 5. Affichage du résumé final (Format requis)
# -----------------------------------------------------------------------------
$vsStatus    = if ($compilerInfo.VSInstalled) { "FOUND ($($compilerInfo.VSName))" } else { "NOT FOUND" }
$msvcStatus  = if ($compilerInfo.MSVCInstalled) { "FOUND ($($compilerInfo.MSVCVersion))" } else { "NOT FOUND" }
$sdkStatus   = if ($compilerInfo.WinSDKInstalled) { "FOUND ($($compilerInfo.WinSDKVersion))" } else { "NOT FOUND" }

$mingwStatus = if ($compilerInfo.MinGWInstalled) { "FOUND" } else { "NOT FOUND" }
$gccVersion  = if ($compilerInfo.MinGWInstalled) { $compilerInfo.GccVersion } else { "NOT FOUND" }
$gppVersion  = if ($compilerInfo.MinGWInstalled) { $compilerInfo.GppVersion } else { "NOT FOUND" }

$occtStatus  = if ($depsInfo.OCCTFound) { "FOUND ($($depsInfo.OCCTVersion) - $($depsInfo.OCCTToolchain))" } else { $depsInfo.OCCTStatus }
$vtkStatus   = if ($depsInfo.VTKFound) { "FOUND ($($depsInfo.VTKVersion) - $($depsInfo.VTKToolchain))" } else { $depsInfo.VTKStatus }

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "TSA Build Environment" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ("{0,-18}: Windows" -f "OS")
Write-Host ("{0,-18}: {1}" -f "Architecture", $archString)
Write-Host ""
Write-Host ("{0,-18}: {1}" -f "Visual Studio", $vsStatus)
Write-Host ("{0,-18}: {1}" -f "MSVC", $msvcStatus)
Write-Host ("{0,-18}: {1}" -f "Windows SDK", $sdkStatus)
Write-Host ""
Write-Host ("{0,-18}: {1}" -f "MinGW-w64", $mingwStatus)
Write-Host ("{0,-18}: {1}" -f "GCC", $gccVersion)
Write-Host ("{0,-18}: {1}" -f "G++", $gppVersion)
Write-Host ""
Write-Host ("{0,-18}: {1}" -f "Qt", $qtInfo.QtVersion)
Write-Host ("{0,-18}: {1}" -f "Qt toolchain", $qtInfo.QtToolchain)
Write-Host ""
Write-Host ("{0,-18}: {1}" -f "OCCT", $occtStatus)
Write-Host ("{0,-18}: {1}" -f "VTK", $vtkStatus)
Write-Host ""
Write-Host ("{0,-18}: {1}" -f "Selected compiler", $selectedCompiler)
Write-Host ("{0,-18}: {1}" -f "CMake generator", $compilerInfo.Generator)
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# -----------------------------------------------------------------------------
# 6. Gestion non-destructive des répertoires de build (Règle 9)
# -----------------------------------------------------------------------------
# Déterminer le dossier de build selon le compilateur
if ($CustomBuildDir) {
    $targetBuildDir = $CustomBuildDir
} else {
    if ($selectedCompiler -eq "MSVC") {
        $targetBuildDir = "build-msvc"
    } else {
        $targetBuildDir = "build-mingw"
    }
}

$fullBuildPath = Join-Path $SourceDir $targetBuildDir

# Vérification du cache existant si build/ classique existe
$defaultBuildPath = Join-Path $SourceDir "build"
$cacheFile = Join-Path $fullBuildPath "CMakeCache.txt"

if (Test-Path $cacheFile) {
    $cacheContent = Get-Content $cacheFile -Raw
    $cacheIsMSVC = ($cacheContent -match 'CMAKE_CXX_COMPILER:FILEPATH=.*cl\.exe' -or $cacheContent -match 'CMAKE_GENERATOR:INTERNAL=Visual Studio')
    $cacheIsMinGW = ($cacheContent -match 'CMAKE_CXX_COMPILER:FILEPATH=.*g\+\+\.exe' -or $cacheContent -match 'CMAKE_GENERATOR:INTERNAL=.*(MinGW|Ninja)')

    if (($selectedCompiler -eq "MSVC" -and $cacheIsMinGW) -or ($selectedCompiler -eq "MinGW" -and $cacheIsMSVC)) {
        Write-Warning "Le dossier '$targetBuildDir' contient un cache configuré pour une autre toolchain."
        # Ne JAMAIS supprimer build/, créer un nouveau dossier propre
        $targetBuildDir = "$targetBuildDir-" + (Get-Date -Format "yyyyMMdd-HHmmss")
        $fullBuildPath = Join-Path $SourceDir $targetBuildDir
        Write-Host "Nouveau dossier de build dédié créé afin de préserver l'ancien : $targetBuildDir" -ForegroundColor Cyan
    }
}

Write-Host "Dossier de build sélectionné : $targetBuildDir" -ForegroundColor Green

# -----------------------------------------------------------------------------
# 7. Configuration de CMake
# -----------------------------------------------------------------------------
$cmakeArgs = @(
    "-B", $fullBuildPath,
    "-S", $SourceDir,
    "-G", $compilerInfo.Generator,
    "-DQt6_DIR=$($qtInfo.Qt6_DIR)",
    "-DCMAKE_BUILD_TYPE=$Config"
)

if ($selectedCompiler -eq "MSVC") {
    $cmakeArgs += @("-A", "x64")
} elseif ($selectedCompiler -eq "MinGW") {
    $cmakeArgs += @(
        "-DCMAKE_C_COMPILER=$($compilerInfo.GccPath)",
        "-DCMAKE_CXX_COMPILER=$($compilerInfo.GppPath)"
    )
    if ($compilerInfo.MakeProgram -and (Test-Path $compilerInfo.MakeProgram)) {
        $cmakeArgs += "-DCMAKE_MAKE_PROGRAM=$($compilerInfo.MakeProgram)"
    }
}

Write-Host "Exécution de CMake :" -ForegroundColor Yellow
Write-Host "cmake $($cmakeArgs -join ' ')" -ForegroundColor Gray
Write-Host ""

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Échec de la configuration CMake."
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "Configuration CMake terminée avec succès !" -ForegroundColor Green

# -----------------------------------------------------------------------------
# 8. Compilation optionnelle si demandée (-Build)
# -----------------------------------------------------------------------------
if ($Build) {
    Write-Host ""
    Write-Host "Démarrage de la compilation ($Config)..." -ForegroundColor Yellow
    & cmake --build $fullBuildPath --config $Config
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Échec de la compilation."
        exit $LASTEXITCODE
    }
    Write-Host "Compilation terminée avec succès !" -ForegroundColor Green
}
