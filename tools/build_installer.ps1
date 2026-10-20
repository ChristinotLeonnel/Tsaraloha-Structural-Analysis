<#
.SYNOPSIS
    Construit, teste et vérifie l'installateur Windows officiel de TSA.
.DESCRIPTION
    1. Vérifie le paquet distribuable (tools/verify_package.py).
    2. Génère les graphismes BMP de l'assistant (tools/generate_installer_graphics.py).
    3. Compile l'installateur avec Inno Setup 6 (ISCC.exe).
    4. Exécute un test d'installation et de désinstallation silencieuse dans un dossier temporaire.
    5. Calcule l'empreinte SHA-256 et la taille exacte en octets.
.PARAMETER BuildDir
    Dossier de construction CMake contenant dist/TSA (par défaut : build-ninja-release).
.PARAMETER SkipTest
    Ignore le test d'installation/désinstallation temporaire.
#>
param(
    [string]$BuildDir = "build-ninja-release",
    [switch]$SkipTest
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")

Write-Host "=== TSA — Construction de l'installateur Windows ===" -ForegroundColor Cyan

# 1. Vérification du paquet distribuable
$distDir = Join-Path $root "$BuildDir\dist\TSA"
if (-not (Test-Path $distDir)) {
    throw "Dossier du paquet introuvable : $distDir. Exécutez d'abord la cible package_TSA."
}

Write-Host "[1/5] Vérification du paquet distribuable..." -ForegroundColor Yellow
& python (Join-Path $root "tools\verify_package.py") $distDir
if ($LASTEXITCODE -ne 0) {
    throw "Échec de vérification du paquet distribuable (code $LASTEXITCODE)."
}

# 2. Graphismes de l'installateur
Write-Host "[2/5] Vérification des graphismes de l'assistant..." -ForegroundColor Yellow
$bannerBmp = Join-Path $root "packaging\wizard_banner.bmp"
$smallBmp = Join-Path $root "packaging\wizard_small.bmp"
if (-not (Test-Path $bannerBmp) -or -not (Test-Path $smallBmp)) {
    & python (Join-Path $root "tools\generate_installer_graphics.py")
}

# 3. Recherche du compilateur Inno Setup
Write-Host "[3/5] Recherche d'Inno Setup (ISCC.exe)..." -ForegroundColor Yellow
$isccCandidates = @(
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    (Get-Command iscc -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue)
) | Where-Object { $_ -and (Test-Path $_) }

if (-not $isccCandidates) {
    throw "Compilateur Inno Setup 6 (ISCC.exe) introuvable. Installez-le avec : winget install JRSoftware.InnoSetup"
}
$iscc = $isccCandidates[0]
Write-Host "Utilisation de : $iscc" -ForegroundColor DarkGray

# Lecture de la version dans ProductIdentity.h
$identityFile = Join-Path $root "product\ProductIdentity.h"
$versionMatch = (Get-Content $identityFile | Select-String 'kVersion\[\] = "([^"]+)"')
if (-not $versionMatch) {
    throw "Impossible de lire kVersion dans $identityFile"
}
$version = $versionMatch.Matches[0].Groups[1].Value

$outputDir = Join-Path $root "$BuildDir\dist"
$setupScript = Join-Path $root "packaging\installer.iss"
$setupIcon = Join-Path $root "resources\icons\TSA.ico"
$licenseFile = Join-Path $root "LICENSE"

# Compilation de l'installateur
Write-Host "[4/5] Compilation de l'installateur pour TSA $version..." -ForegroundColor Yellow
$isccArgs = @(
    "/Q",
    "/DProduct=TSA",
    "/DProductSlug=tsa",
    "/DVersion=$version",
    "/DNumericVersion=$version",
    "/DAppId=15E32277-3248-46AD-9261-B3098FF290A2",
    "/DAppUserModelId=TSAEngineering.TSA.StructuralModeler.1.0",
    "/DExtension=.tsa",
    "/DProgId=TSA.Project",
    "/DFriendlyName=TSA Project File",
    "/DContentType=application/x-tsa-project",
    "/DThumbnailDll=TSAThumbnailProvider.dll",
    "/DDataDir=TSA Engineering\TSA",
    "/DSourceDir=$distDir",
    "/DOutputDir=$outputDir",
    "/DSetupIcon=$setupIcon",
    "/DLicenseFile=$licenseFile",
    $setupScript
)

& $iscc @isccArgs
if ($LASTEXITCODE -ne 0) {
    throw "Échec de la compilation Inno Setup (code $LASTEXITCODE)."
}

$setupExe = Join-Path $outputDir "TSA-$version-windows-x64-setup.exe"
if (-not (Test-Path $setupExe)) {
    throw "L'exécutable d'installation produit est introuvable : $setupExe"
}

$size = (Get-Item $setupExe).Length
$sha256 = (Get-FileHash -Algorithm SHA256 $setupExe).Hash.ToLower()
$sizeMb = [math]::Round($size / (1024 * 1024), 1)

Write-Host "Installateur généré : $setupExe" -ForegroundColor Green
Write-Host "  Taille   : $size octets ($sizeMb Mo)" -ForegroundColor Green
Write-Host "  SHA-256  : $sha256" -ForegroundColor Green

# 5. Test d'installation et de désinstallation temporaire
if (-not $SkipTest) {
    Write-Host "[5/5] Test réel d'installation et de désinstallation silencieuse..." -ForegroundColor Yellow
    $testDir = Join-Path $env:TEMP "tsa-installer-validation-test"
    if (Test-Path $testDir) { Remove-Item -Recurse -Force $testDir }
    
    $logFile = Join-Path $env:TEMP "tsa-installer-test.log"
    $proc = Start-Process $setupExe -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/DIR=`"$testDir`"", "/LOG=`"$logFile`"" -Wait -PassThru
    if ($proc.ExitCode -ne 0) {
        Get-Content $logFile -Tail 30
        throw "Échec de l'installation silencieuse (code $($proc.ExitCode))."
    }

    # Vérification des fichiers critiques
    $criticalFiles = @("TSA.exe", "MANIFEST.json", "gmsh-4.15.dll", "Qt6Core.dll", "unins000.exe")
    foreach ($f in $criticalFiles) {
        $p = Join-Path $testDir $f
        if (-not (Test-Path $p)) {
            throw "Fichier critique absent après installation : $f"
        }
    }
    Write-Host "  Fichiers critiques vérifiés dans $testDir : OK" -ForegroundColor Green

    # Test désinstallation
    $unins = Join-Path $testDir "unins000.exe"
    $uProc = Start-Process $unins -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART" -Wait -PassThru
    if ($uProc.ExitCode -ne 0) {
        throw "Échec de la désinstallation silencieuse (code $($uProc.ExitCode))."
    }
    Start-Sleep -Seconds 2
    if (Test-Path (Join-Path $testDir "TSA.exe")) {
        throw "Désinstallation incomplète : TSA.exe est toujours présent."
    }
    Remove-Item -Recurse -Force $testDir -ErrorAction SilentlyContinue
    Write-Host "  Désinstallation et nettoyage complet : OK" -ForegroundColor Green
}

Write-Host "`nSuccès ! L'installateur officiel est prêt." -ForegroundColor Cyan

# Émission des métadonnées JSON
[PSCustomObject]@{
    Product     = "TSA"
    Version     = $version
    File        = (Split-Path $setupExe -Leaf)
    FullPath    = $setupExe
    Size        = $size
    SizeMb      = $sizeMb
    Sha256      = $sha256
    GeneratedAt = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
} | ConvertTo-Json
