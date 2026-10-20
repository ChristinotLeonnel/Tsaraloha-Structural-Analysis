# tools/build_installer.ps1
# Automatise la verification, la generation des graphismes, la compilation et le test de l'installateur TSA.

[CmdletBinding()]
param(
    [string]$BuildDir = "build-ninja-release",
    [string]$OutputDir,
    [switch]$SkipTest
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Tsaraloha Structural Analysis - Windows Installer Builder" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Verification du paquet distribuable
$distDir = Join-Path $root "$BuildDir\dist\TSA"
if (-not (Test-Path $distDir)) {
    throw ("Dossier du paquet introuvable : " + $distDir)
}

Write-Host "[1/5] Verification du paquet distribuable..." -ForegroundColor Yellow
& python (Join-Path $root "tools\verify_package.py") $distDir
if ($LASTEXITCODE -ne 0) {
    throw ("Echec de verification du paquet distribuable, code: " + $LASTEXITCODE)
}

# 2. Graphismes officiels de l'installateur avec logo T
Write-Host "[2/5] Generation des graphismes officiels avec logo T..." -ForegroundColor Yellow
& python (Join-Path $root "tools\generate_installer_graphics.py")

# 3. Recherche du compilateur Inno Setup
$isccList = @(
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    "C:\Users\TSARALOHA Christinot\AppData\Local\Programs\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
)
$iscc = $null
foreach ($cand in $isccList) {
    if ($cand -and (Test-Path $cand)) {
        $iscc = $cand
        break
    }
}

if (-not $iscc) {
    throw "Compilateur Inno Setup 6 (ISCC.exe) introuvable."
}
Write-Host ("Utilisation de : " + $iscc) -ForegroundColor DarkGray

# Lecture de la version dans ProductIdentity.h
$identityFile = Join-Path $root "product\ProductIdentity.h"
$vLine = Get-Content $identityFile | Where-Object { $_.Contains("kVersion") } | Select-Object -First 1
if ($vLine) {
    $version = $vLine.Split('"')[1]
} else {
    throw "Impossible de lire kVersion dans ProductIdentity.h"
}

if (-not $OutputDir) {
    $OutputDir = Join-Path $root "$BuildDir\dist"
}
$setupScript = Join-Path $root "packaging\installer.iss"
$setupIcon = Join-Path $root "resources\icons\TSA.ico"
$licenseFile = Join-Path $root "LICENSE"

# Compilation de l'installateur
Write-Host ("[4/5] Compilation de l'installateur pour TSA " + $version + "...") -ForegroundColor Yellow
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
    "/DOutputDir=$OutputDir",
    "/DSetupIcon=$setupIcon",
    "/DLicenseFile=$licenseFile",
    $setupScript
)

& $iscc @isccArgs
if ($LASTEXITCODE -ne 0) {
    throw ("Echec de la compilation Inno Setup, code: " + $LASTEXITCODE)
}

$setupExe = Join-Path $OutputDir ("TSA-" + $version + "-windows-x64-setup.exe")
if (-not (Test-Path $setupExe)) {
    throw ("L'executable d'installation produit est introuvable : " + $setupExe)
}

$size = (Get-Item $setupExe).Length
$sha256 = (Get-FileHash -Algorithm SHA256 $setupExe).Hash.ToLower()
$sizeMb = [math]::Round($size / (1024 * 1024), 1)

Write-Host ("Installateur genere : " + $setupExe) -ForegroundColor Green
Write-Host ("  Taille   : {0} octets ({1} Mo)" -f $size, $sizeMb) -ForegroundColor Green
Write-Host ("  SHA-256  : " + $sha256) -ForegroundColor Green

# 5. Test d'installation et de desinstallation temporaire
if (-not $SkipTest) {
    Write-Host "[5/5] Test reel d'installation et de desinstallation silencieuse..." -ForegroundColor Yellow
    $testDir = Join-Path $env:TEMP "tsa-installer-validation-test"
    if (Test-Path $testDir) { Remove-Item -Recurse -Force $testDir }
    
    $logFile = Join-Path $env:TEMP "tsa-installer-test.log"
    $proc = Start-Process $setupExe -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/DIR=`"$testDir`"", "/LOG=`"$logFile`"" -Wait -PassThru
    if ($proc.ExitCode -ne 0) {
        throw ("Echec de l'installation silencieuse, code: " + $proc.ExitCode)
    }

    # Verification des fichiers critiques
    $criticalFiles = @("TSA.exe", "MANIFEST.json", "gmsh-4.15.dll", "Qt6Core.dll", "unins000.exe")
    foreach ($f in $criticalFiles) {
        $p = Join-Path $testDir $f
        if (-not (Test-Path $p)) {
            throw ("Fichier critique absent apres installation : " + $f)
        }
    }
    Write-Host ("  Fichiers critiques verifies dans " + $testDir + " : OK") -ForegroundColor Green

    # Test desinstallation
    $unins = Join-Path $testDir "unins000.exe"
    $uProc = Start-Process $unins -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART" -Wait -PassThru
    if ($uProc.ExitCode -ne 0) {
        throw ("Echec de la desinstallation silencieuse, code: " + $uProc.ExitCode)
    }
    Start-Sleep -Seconds 2
    if (Test-Path (Join-Path $testDir "TSA.exe")) {
        throw "Desinstallation incomplete : TSA.exe est toujours present."
    }
    Remove-Item -Recurse -Force $testDir -ErrorAction SilentlyContinue
    Write-Host "  Desinstallation et nettoyage complet : OK" -ForegroundColor Green
}

Write-Host "`nSucces ! L'installateur officiel TSA avec le logo T est pret." -ForegroundColor Cyan

# Emission des metadonnees JSON
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
