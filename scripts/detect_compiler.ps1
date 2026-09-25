# =============================================================================
# scripts/detect_compiler.ps1
# Détection et validation rigoureuse des compilateurs Windows (MSVC et MinGW-w64)
# =============================================================================

[CmdletBinding()]
param(
    [switch]$ForceMinGW,
    [string]$PreferredMinGWPath = "",
    [switch]$VerboseOutput
)

$Result = [PSCustomObject]@{
    SelectedCompiler = "None"
    Available        = $false
    VSInstalled      = $false
    VSName           = "NOT FOUND"
    VSVersion        = "NOT FOUND"
    MSVCInstalled    = $false
    MSVCVersion      = "NOT FOUND"
    WinSDKInstalled  = $false
    WinSDKVersion    = "NOT FOUND"
    VCVarsPath       = ""
    ClPath           = ""
    MinGWInstalled   = $false
    GccVersion       = "NOT FOUND"
    GppVersion       = "NOT FOUND"
    GccPath          = ""
    GppPath          = ""
    MinGWBin         = ""
    Generator        = ""
    MakeProgram      = ""
}

# -----------------------------------------------------------------------------
# Fonction de test de compilation C++20
# -----------------------------------------------------------------------------
function Test-CppCompilation {
    param(
        [string]$CompilerType, # "MSVC" ou "MinGW"
        [string]$CompilerExe,
        [string]$VCVarsScript = ""
    )
    $tempDir = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), "tsa_test_" + [System.Guid]::NewGuid().ToString("N"))
    [System.IO.Directory]::CreateDirectory($tempDir) | Out-Null
    $srcFile = Join-Path $tempDir "test.cpp"
    $exeFile = Join-Path $tempDir "test.exe"

    $code = @"
#include <iostream>
int main() {
    std::cout << 42 << std::endl;
    return 0;
}
"@
    Set-Content -Path $srcFile -Value $code -Encoding ASCII

    $success = $false
    $prevDir = Get-Location
    try {
        Set-Location $tempDir
        if ($CompilerType -eq "MSVC") {
            # Compilation sous environnement vcvarsall x64
            $batchCmd = "@echo off`ncall `"$VCVarsScript`" x64 >nul 2>&1`ncl.exe /nologo /std:c++20 /EHsc test.cpp /Fe:test.exe >nul 2>&1`n"
            $batchFile = Join-Path $tempDir "compile.bat"
            Set-Content -Path $batchFile -Value $batchCmd -Encoding ASCII
            & cmd.exe /c "`"$batchFile`""
            if (Test-Path $exeFile) {
                $out = & $exeFile
                if ($out -and ($out.Trim() -eq "42")) { $success = $true }
            }
        } elseif ($CompilerType -eq "MinGW") {
            # Si le compilateur n'a pas ses DLLs dans PATH, ajouter temporairement son dossier bin
            $binDir = Split-Path $CompilerExe -Parent
            $oldPath = $env:PATH
            if ($env:PATH -notlike "*$binDir*") {
                $env:PATH = "$binDir;$env:PATH"
            }
            try {
                & $CompilerExe -std=c++20 test.cpp -o test.exe 2>&1 | Out-Null
                if (Test-Path $exeFile) {
                    $out = & $exeFile 2>&1
                    if ($out -and ($out.Trim() -eq "42")) { $success = $true }
                }
            } finally {
                $env:PATH = $oldPath
            }
        }
    } catch {
        $success = $false
    } finally {
        Set-Location $prevDir
        if (Test-Path $tempDir) {
            Remove-Item -Path $tempDir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    return $success
}

# -----------------------------------------------------------------------------
# 1. Détection de Visual Studio et MSVC (sauf si ForceMinGW activé)
# -----------------------------------------------------------------------------
if (-not $ForceMinGW) {
    $vswherePaths = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
    )

    $vswhere = $vswherePaths | Where-Object { Test-Path $_ } | Select-Object -First 1

    if ($vswhere) {
        try {
            $vsInstances = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json
            if ($vsInstances -and $vsInstances.Count -gt 0) {
                $inst = $vsInstances[0]
                $vsInstallPath = $inst.installationPath
                $Result.VSInstalled = $true
                $Result.VSName = $inst.displayName
                $Result.VSVersion = $inst.installationVersion

                # Vérification de vcvarsall.bat
                $vcvars = Join-Path $vsInstallPath "VC\Auxiliary\Build\vcvarsall.bat"
                if (Test-Path $vcvars) {
                    $Result.VCVarsPath = $vcvars
                }

                # Recherche de la version de MSVC
                $msvcRootDir = Join-Path $vsInstallPath "VC\Tools\MSVC"
                if (Test-Path $msvcRootDir) {
                    $msvcVersions = Get-ChildItem -Directory $msvcRootDir | Sort-Object Name -Descending
                    if ($msvcVersions.Count -gt 0) {
                        $latestMSVC = $msvcVersions[0].FullName
                        $clExe = Join-Path $latestMSVC "bin\Hostx64\x64\cl.exe"
                        if (Test-Path $clExe) {
                            $Result.MSVCInstalled = $true
                            $Result.MSVCVersion = $msvcVersions[0].Name
                            $Result.ClPath = $clExe
                        }
                    }
                }

                # Recherche du Windows SDK
                $winKitsDir = "${env:ProgramFiles(x86)}\Windows Kits\10\Include"
                if (Test-Path $winKitsDir) {
                    $sdkVersions = Get-ChildItem -Directory $winKitsDir | Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } | Sort-Object Name -Descending
                    if ($sdkVersions.Count -gt 0) {
                        $Result.WinSDKInstalled = $true
                        $Result.WinSDKVersion = $sdkVersions[0].Name
                    }
                }

                # Validation de la compilation MSVC
                if ($Result.MSVCInstalled -and $Result.WinSDKInstalled -and $Result.VCVarsPath) {
                    $msvcWorks = Test-CppCompilation -CompilerType "MSVC" -CompilerExe $Result.ClPath -VCVarsScript $Result.VCVarsPath
                    if ($msvcWorks) {
                        $Result.SelectedCompiler = "MSVC"
                        $Result.Available = $true

                        # Sélection du générateur CMake
                        $majorVS = $Result.VSVersion.Split('.')[0]
                        if ($majorVS -eq "18") {
                            $Result.Generator = "Visual Studio 18 2026"
                        } elseif ($majorVS -eq "17") {
                            $Result.Generator = "Visual Studio 17 2022"
                        } elseif ($majorVS -eq "16") {
                            $Result.Generator = "Visual Studio 16 2019"
                        } else {
                            $Result.Generator = "Visual Studio 17 2022"
                        }

                        Write-Host "Compiler sélectionné : MSVC" -ForegroundColor Green
                        return $Result
                    }
                }
            }
        } catch {
            Write-Verbose "Erreur lors de la détection MSVC : $_"
        }
    }
}

# -----------------------------------------------------------------------------
# 2. Si MSVC n'est pas disponible : recherche de MinGW-w64
# -----------------------------------------------------------------------------
if (-not $ForceMinGW) {
    Write-Host "Visual Studio/MSVC introuvable." -ForegroundColor Yellow
}
Write-Host "Recherche de MinGW-w64..." -ForegroundColor Cyan

$searchDirs = @()
if ($PreferredMinGWPath -and (Test-Path $PreferredMinGWPath)) {
    $searchDirs += $PreferredMinGWPath
}

# Chemins standards MinGW
$searchDirs += @(
    "C:\TSA\tools\mingw64\bin",
    "$PSScriptRoot\..\tools\mingw64\bin",
    "$env:LOCALAPPDATA\TSA\tools\mingw64\bin",
    "C:\mingw64\bin",
    "C:\tools\mingw64\bin"
)

# Chemins Qt Tools (souvent dotés d'un MinGW officiel et fonctionnel)
if (Test-Path "C:\Qt\Tools") {
    $qtMingw = Get-ChildItem -Directory "C:\Qt\Tools" -Filter "mingw*" | ForEach-Object { Join-Path $_.FullName "bin" }
    $searchDirs += $qtMingw
}

# Ajout du PATH actuel
$envPaths = $env:PATH -split ';' | Where-Object { $_ -and (Test-Path $_) }
$searchDirs += $envPaths

$foundGcc = $null
$foundGpp = $null

foreach ($dir in $searchDirs) {
    $gccTest = Join-Path $dir "gcc.exe"
    $gppTest = Join-Path $dir "g++.exe"
    if ((Test-Path $gccTest) -and (Test-Path $gppTest)) {
        try {
            $verOutput = & $gppTest --version 2>&1 | Out-String
            if ($verOutput -match '\b(\d{1,2}\.\d+\.\d+)\b') {
                $foundGcc = $gccTest
                $foundGpp = $gppTest
                $Result.MinGWBin = $dir
                $Result.GccPath = $foundGcc
                $Result.GppPath = $foundGpp
                $Result.GppVersion = $Matches[1]
                $Result.GccVersion = $Matches[1]
                $Result.MinGWInstalled = $true
                break
            }
        } catch {}
    }
}

if ($Result.MinGWInstalled) {
    # Validation réelle par compilation C++20
    $mingwWorks = Test-CppCompilation -CompilerType "MinGW" -CompilerExe $Result.GppPath
    if ($mingwWorks) {
        $Result.SelectedCompiler = "MinGW"
        $Result.Available = $true

        # Détection du générateur et du make program
        $ninja = Get-Command "ninja.exe" -ErrorAction SilentlyContinue
        $ninjaExe = ""
        if ($ninja) {
            $ninjaExe = $ninja.Source
        } elseif (Test-Path "C:\Qt\Tools\Ninja\ninja.exe") {
            $ninjaExe = "C:\Qt\Tools\Ninja\ninja.exe"
        }

        $makeExe = Join-Path $Result.MinGWBin "mingw32-make.exe"
        if (-not (Test-Path $makeExe)) {
            $makeFound = Get-Command "mingw32-make.exe" -ErrorAction SilentlyContinue
            if ($makeFound) { $makeExe = $makeFound.Source }
        }

        if ($ninjaExe -and (Test-Path $ninjaExe)) {
            $Result.Generator = "Ninja"
            $Result.MakeProgram = $ninjaExe
        } elseif ($makeExe -and (Test-Path $makeExe)) {
            $Result.Generator = "MinGW Makefiles"
            $Result.MakeProgram = $makeExe
        } else {
            $Result.Generator = "MinGW Makefiles"
            $Result.MakeProgram = ""
        }

        Write-Host "Compiler sélectionné : MinGW ($($Result.GppVersion))" -ForegroundColor Green
        return $Result
    } else {
        Write-Warning "MinGW trouvé dans $($Result.MinGWBin) mais a échoué au test de compilation C++20."
        $Result.MinGWInstalled = $false
    }
}

return $Result
