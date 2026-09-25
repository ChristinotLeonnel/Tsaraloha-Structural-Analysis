# =============================================================================
# scripts/install_mingw.ps1
# Téléchargement, vérification SHA256 et installation automatique de MinGW-w64
# =============================================================================

[CmdletBinding()]
param(
    [string]$InstallDir = "C:\TSA\tools\mingw64",
    [switch]$ForceInstall,
    [switch]$SkipCheckMSVC
)

# -----------------------------------------------------------------------------
# 1. Ne jamais installer MinGW si MSVC est fonctionnel (Règle d'or)
# -----------------------------------------------------------------------------
if (-not $SkipCheckMSVC) {
    $detectScript = Join-Path $PSScriptRoot "detect_compiler.ps1"
    if (Test-Path $detectScript) {
        $detection = & $detectScript
        if ($detection.SelectedCompiler -eq "MSVC" -and $detection.Available) {
            Write-Host "Un environnement MSVC fonctionnel est disponible. MinGW ne sera pas installé." -ForegroundColor Green
            return $null
        }
    }
}

# -----------------------------------------------------------------------------
# 2. Vérification de l'architecture du système (Windows 64 bits requis)
# -----------------------------------------------------------------------------
if (-not [Environment]::Is64BitOperatingSystem) {
    Write-Error "Erreur : MinGW-w64 requiert un système d'exploitation Windows 64 bits."
    exit 1
}

# -----------------------------------------------------------------------------
# 3. Vérification d'une installation existante (ne pas écraser sans confirmation)
# -----------------------------------------------------------------------------
$existingGcc = Join-Path $InstallDir "bin\gcc.exe"
$existingGpp = Join-Path $InstallDir "bin\g++.exe"

if ((Test-Path $existingGcc) -and (Test-Path $existingGpp) -and (-not $ForceInstall)) {
    Write-Host "Une installation existante de MinGW-w64 a été détectée dans : $InstallDir" -ForegroundColor Cyan
    $testOut = & $existingGcc --version 2>&1 | Select-Object -First 1
    Write-Host "Version détectée : $testOut" -ForegroundColor Green
    return (Join-Path $InstallDir "bin")
}

# -----------------------------------------------------------------------------
# 4. Détermination de l'emplacement cible avec gestion des permissions
# -----------------------------------------------------------------------------
$targetParent = Split-Path $InstallDir -Parent
try {
    if (-not (Test-Path $targetParent)) {
        New-Item -ItemType Directory -Path $targetParent -Force -ErrorAction Stop | Out-Null
    }
    # Test d'écriture
    $testFile = Join-Path $targetParent (".perm_test_" + [Guid]::NewGuid().ToString("N"))
    [System.IO.File]::WriteAllText($testFile, "test")
    Remove-Item $testFile -Force -ErrorAction SilentlyContinue
} catch {
    Write-Warning "Droits d'écriture insuffisants sur '$InstallDir'. Bascule vers le dossier utilisateur."
    $InstallDir = Join-Path $env:LOCALAPPDATA "TSA\tools\mingw64"
    $targetParent = Split-Path $InstallDir -Parent
    if (-not (Test-Path $targetParent)) {
        New-Item -ItemType Directory -Path $targetParent -Force | Out-Null
    }
}

Write-Host "Emplacement d'installation MinGW-w64 : $InstallDir" -ForegroundColor Cyan

# -----------------------------------------------------------------------------
# 5. Métadonnées officielles du package MinGW-w64 (WinLibs UCRT x64)
# -----------------------------------------------------------------------------
# Source officielle, fiable et maintenue (Brecht Sanders / WinLibs)
$PackageUrl = "https://github.com/brechtsanders/winlibs_mingw/releases/download/16.2.0posix-14.0.0-ucrt-r1/winlibs-x86_64-posix-seh-gcc-16.2.0-mingw-w64ucrt-14.0.0-r1.zip"
$ExpectedSha256 = "c1f52294597c0b73786b2a78eb5d176d89226d2f21875eab75e783a8b1cefcc4"

$tempZip = Join-Path $env:TEMP ("mingw64_download_" + [Guid]::NewGuid().ToString("N") + ".zip")
$tempExtract = Join-Path $env:TEMP ("mingw64_extract_" + [Guid]::NewGuid().ToString("N"))

try {
    # -------------------------------------------------------------------------
    # 6. Téléchargement avec indicateur de progression
    # -------------------------------------------------------------------------
    Write-Host "Téléchargement de MinGW-w64 (GCC 16.2.0 + UCRT + SEH 64-bit)..." -ForegroundColor Yellow
    Write-Host "Source : $PackageUrl"

    $downloadSuccess = $false
    # Utilisation de curl.exe si disponible (plus rapide avec barre de progression native)
    $curl = Get-Command "curl.exe" -ErrorAction SilentlyContinue
    if ($curl) {
        & curl.exe -L --progress-bar -o "$tempZip" "$PackageUrl"
        if ($LASTEXITCODE -eq 0 -and (Test-Path $tempZip)) {
            $downloadSuccess = $true
        }
    }

    if (-not $downloadSuccess) {
        Write-Host "Utilisation de l'API Web PowerShell..."
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -Uri $PackageUrl -OutFile $tempZip -UseBasicParsing
        if (Test-Path $tempZip) { $downloadSuccess = $true }
    }

    if (-not $downloadSuccess) {
        Write-Error "Échec du téléchargement de MinGW-w64."
        exit 1
    }

    # -------------------------------------------------------------------------
    # 7. Vérification de l'intégrité SHA256 officielle
    # -------------------------------------------------------------------------
    Write-Host "Vérification de l'empreinte SHA256 officielle..." -ForegroundColor Yellow
    $actualSha256 = (Get-FileHash -Path $tempZip -Algorithm SHA256).Hash.ToLower()

    if ($actualSha256 -ne $ExpectedSha256.ToLower()) {
        Write-Error "ÉCHEC D'INTÉGRITÉ SHA256 !`nAttendu : $ExpectedSha256`nCalculé : $actualSha256"
        Remove-Item -Path $tempZip -Force -ErrorAction SilentlyContinue
        exit 1
    }
    Write-Host "Empreinte SHA256 validée avec succès !" -ForegroundColor Green

    # -------------------------------------------------------------------------
    # 8. Extraction contrôlée
    # -------------------------------------------------------------------------
    Write-Host "Extraction de MinGW-w64 vers $InstallDir ..." -ForegroundColor Yellow
    [System.IO.Directory]::CreateDirectory($tempExtract) | Out-Null

    # Utilisation de tar.exe natif Windows ou Expand-Archive
    $tar = Get-Command "tar.exe" -ErrorAction SilentlyContinue
    if ($tar) {
        & tar.exe -xf "$tempZip" -C "$tempExtract"
    } else {
        Expand-Archive -Path $tempZip -DestinationPath $tempExtract -Force
    }

    # Déplacement vers l'emplacement final
    if (-not (Test-Path $InstallDir)) {
        [System.IO.Directory]::CreateDirectory($InstallDir) | Out-Null
    }

    $extractedRoot = Join-Path $tempExtract "mingw64"
    if (Test-Path $extractedRoot) {
        Copy-Item -Path "$extractedRoot\*" -Destination $InstallDir -Recurse -Force
    } else {
        Copy-Item -Path "$tempExtract\*" -Destination $InstallDir -Recurse -Force
    }

} finally {
    # Nettoyage des fichiers temporaires
    if (Test-Path $tempZip) { Remove-Item -Path $tempZip -Force -ErrorAction SilentlyContinue }
    if (Test-Path $tempExtract) { Remove-Item -Path $tempExtract -Recurse -Force -ErrorAction SilentlyContinue }
}

# -----------------------------------------------------------------------------
# 9. Configuration du PATH et validation finale
# -----------------------------------------------------------------------------
$binDir = Join-Path $InstallDir "bin"
$installedGcc = Join-Path $binDir "gcc.exe"
$installedGpp = Join-Path $binDir "g++.exe"
$installedMake = Join-Path $binDir "mingw32-make.exe"

if (-not ((Test-Path $installedGcc) -and (Test-Path $installedGpp))) {
    Write-Error "Erreur : Les fichiers exécutables gcc/g++ sont introuvables après l'installation."
    exit 1
}

# Ajout au PATH du processus sans doublon
$currentPaths = $env:PATH -split ';'
if ($currentPaths -notcontains $binDir) {
    $env:PATH = "$binDir;$env:PATH"
    Write-Host "Dossier bin ajouté au PATH de la session courante : $binDir" -ForegroundColor Cyan
}

# Vérification finale
Write-Host "========================================" -ForegroundColor Green
Write-Host "MinGW-w64 installé et validé avec succès !" -ForegroundColor Green
& $installedGcc --version | Select-Object -First 1
& $installedGpp --version | Select-Object -First 1
if (Test-Path $installedMake) {
    & $installedMake --version | Select-Object -First 1
}
Write-Host "========================================" -ForegroundColor Green

return $binDir
