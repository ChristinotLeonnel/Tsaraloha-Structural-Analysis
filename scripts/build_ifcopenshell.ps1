# =============================================================================
# scripts/build_ifcopenshell.ps1
# Compile IfcOpenShell (lecture IFC et noyau géométrique OpenCASCADE) contre l'OCCT de TSA, avec ses dépendances
# Boost et Eigen, et l'installe dans thirdparty/ (docs/IFC_MAPPING.md, « Bibliothèque IFC »).
#
# Versions figées, archives vérifiées par leur empreinte SHA-256 :
#   IfcOpenShell 0.9.0post1 (LGPL-3.0)  Boost 1.92.0 (BSL-1.0)  Eigen 3.4.1 (MPL-2.0)
# Composants exclus : Python, IfcConvert, CGAL, COLLADA, glTF, USD, PROJ (licences et taille).
#
# Usage (PowerShell, depuis la racine de TSA) :
#   .\scripts\build_ifcopenshell.ps1 [-OcctRoot <dossier opencascade-8.0.1-vc14-64>] [-Prefix thirdparty]
# Le compilateur MSVC est pris dans l'environnement courant (Developer PowerShell, msvc-dev-cmd) ou, à défaut,
# chargé depuis Visual Studio (vswhere).
# =============================================================================

[CmdletBinding()]
param(
    [string]$OcctRoot = "",
    [string]$Prefix = "",
    [string]$WorkDir = "",
    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"
$SourceDir = Split-Path $PSScriptRoot -Parent
if (-not $OcctRoot) { $OcctRoot = Join-Path $SourceDir "opencascade-8.0.1-vc14-64" }
if (-not $Prefix) { $Prefix = Join-Path $SourceDir "thirdparty" }
if (-not $WorkDir) { $WorkDir = Join-Path $Prefix "_build-ifcopenshell" }
if ($Jobs -le 0) { $Jobs = [Environment]::ProcessorCount }

$Sources = @(
    @{ Name = "boost"; File = "boost-1.92.0.tar.gz"; Dir = "boost-1.92.0"
       Url = "https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-b2-nodocs.tar.gz"
       Sha256 = "e2a814b3a158ab482c7a3d330f8bf5a7a8423d258a4e2fb396996e00fcee2111" },
    @{ Name = "eigen"; File = "eigen-3.4.1.tar.gz"; Dir = "eigen-3.4.1"
       Url = "https://gitlab.com/libeigen/eigen/-/archive/3.4.1/eigen-3.4.1.tar.gz"
       Sha256 = "b93c667d1b69265cdb4d9f30ec21f8facbbe8b307cf34c0b9942834c6d4fdbe2" },
    @{ Name = "ifcopenshell"; File = "ifcopenshell-0.9.0post1.tar.gz"; Dir = "IfcOpenShell-ifcopenshell-python-0.9.0post1"
       Url = "https://github.com/IfcOpenShell/IfcOpenShell/archive/refs/tags/ifcopenshell-python-0.9.0post1.tar.gz"
       Sha256 = "2776eef7ddc0137df317f0294623523dc4ccdaba468c172232cfdda13f4e912a" }
)
$BoostPrefix = Join-Path $Prefix "boost-1.92.0"
$EigenPrefix = Join-Path $Prefix "eigen-3.4.1"
$IfcPrefix = Join-Path $Prefix "ifcopenshell-0.9.0post1"

function Step([string]$text) { Write-Host "=== $text" -ForegroundColor Cyan }

function Invoke-Native([string]$exe, [string[]]$arguments, [string]$log) {
    # Journal par étape ; en cas d'échec, sa fin est affichée et le script s'arrête. Le code de retour fait foi :
    # sous PowerShell 5.1, un message sur la sortie d'erreur d'un programme natif serait sinon fatal.
    $ErrorActionPreference = "Continue"
    & $exe @arguments *> $log
    if ($LASTEXITCODE -ne 0) {
        Get-Content $log -Tail 40 | Write-Host
        throw "Échec : $exe (journal : $log)"
    }
}

# --- 0. Environnement MSVC ---------------------------------------------------------------------------------
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "MSVC introuvable : lancer depuis un Developer PowerShell (vcvars64)." }
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
    Step "Environnement MSVC : $vcvars"
    cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match "^([^=]+)=(.*)$") { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
    }
}
if (-not (Test-Path (Join-Path $OcctRoot "inc\Standard_Version.hxx"))) { throw "OCCT introuvable : $OcctRoot" }
# Les scripts de Boost s'appellent sans « .\ » : le dossier courant doit être parcouru.
Remove-Item Env:\NoDefaultCurrentDirectoryInExePath -ErrorAction SilentlyContinue

New-Item -ItemType Directory -Force (Join-Path $WorkDir "dl"), (Join-Path $WorkDir "src"), $Prefix | Out-Null

# --- 1. Téléchargement et vérification ------------------------------------------------------------------------
foreach ($s in $Sources) {
    $archive = Join-Path $WorkDir "dl\$($s.File)"
    if (-not (Test-Path $archive)) {
        Step "Téléchargement $($s.Url)"
        Invoke-WebRequest -Uri $s.Url -OutFile $archive -UseBasicParsing
    }
    $hash = (Get-FileHash -Algorithm SHA256 $archive).Hash.ToLower()
    if ($hash -ne $s.Sha256) {
        Remove-Item $archive
        throw "Empreinte inattendue pour $($s.File) : $hash (attendu $($s.Sha256)). Archive supprimée."
    }
    $dir = Join-Path $WorkDir "src\$($s.Dir)"
    if (-not (Test-Path $dir)) {
        Step "Extraction $($s.File)"
        # IfcOpenShell : quelques fichiers hors compilation C++ (liens de nix/, gabarits Bonsai) ne s'extraient
        # pas sous Windows ; le contrôle porte sur les dossiers compilés.
        $ErrorActionPreference = "Continue"
        & tar -xzf $archive -C (Join-Path $WorkDir "src") 2>&1 | Out-Null
        $ErrorActionPreference = "Stop"
        if (-not (Test-Path $dir)) { throw "Extraction incomplète : $dir" }
    }
}
$IfcSrc = Join-Path $WorkDir "src\IfcOpenShell-ifcopenshell-python-0.9.0post1"
foreach ($d in "cmake", "src\ifcparse", "src\ifcgeom", "src\serializers") {
    if (-not (Test-Path (Join-Path $IfcSrc $d))) { throw "Sources IfcOpenShell incomplètes : $d" }
}

# --- 2. Boost : program_options, regex (statiques, runtime partagé) ------------------------------------------
if (-not (Test-Path (Join-Path $BoostPrefix "lib\libboost_program_options.lib"))) {
    Step "Boost 1.92.0"
    Push-Location (Join-Path $WorkDir "src\boost-1.92.0")
    try {
        Invoke-Native "cmd.exe" @("/c", ".\bootstrap.bat") (Join-Path $WorkDir "boost-bootstrap.log")
        Invoke-Native ".\b2.exe" @("-j$Jobs", "--with-program_options", "--with-regex", "link=static", "runtime-link=shared",
            "threading=multi", "address-model=64", "variant=release", "--layout=system", "--prefix=$BoostPrefix", "install") (Join-Path $WorkDir "boost-b2.log")
    } finally { Pop-Location }
}

# --- 3. Eigen : installation des en-têtes et de Eigen3Config.cmake ---------------------------------------------
if (-not (Test-Path (Join-Path $EigenPrefix "share\eigen3\cmake\Eigen3Config.cmake"))) {
    Step "Eigen 3.4.1"
    Invoke-Native "cmake" @("-S", (Join-Path $WorkDir "src\eigen-3.4.1"), "-B", (Join-Path $WorkDir "build-eigen"), "-G", "Ninja",
        "-DBUILD_TESTING=OFF", "-DEIGEN_BUILD_DOC=OFF", "-DCMAKE_INSTALL_PREFIX=$EigenPrefix") (Join-Path $WorkDir "eigen-configure.log")
    Invoke-Native "cmake" @("--install", (Join-Path $WorkDir "build-eigen")) (Join-Path $WorkDir "eigen-install.log")
}

# --- 4. IfcOpenShell : IfcParse + IfcGeom (noyau OpenCASCADE), schémas IFC2X3, IFC4, IFC4X3_ADD2 ---------------
Step "IfcOpenShell 0.9.0post1 (configuration)"
# OCCT par ses dossiers (et non sa configuration CMake, qui exige VTK pour des modules inutilisés ici).
Invoke-Native "cmake" @("-S", (Join-Path $IfcSrc "cmake"), "-B", (Join-Path $WorkDir "build-ifcopenshell"), "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=$IfcPrefix",
    "-DBUILD_IFCPYTHON=OFF", "-DBUILD_CONVERT=OFF", "-DBUILD_GEOMSERVER=OFF", "-DBUILD_EXAMPLES=OFF", "-DBUILD_DOCUMENTATION=OFF",
    "-DWITH_CGAL=OFF", "-DWITH_MANIFOLD=OFF", "-DCOLLADA_SUPPORT=OFF", "-DGLTF_SUPPORT=OFF", "-DUSD_SUPPORT=OFF", "-DWITH_PROJ=OFF",
    "-DSCHEMA_VERSIONS=2x3;4;4x3_add2",
    "-DOCC_INCLUDE_DIR=$(Join-Path $OcctRoot 'inc')", "-DOCC_LIBRARY_DIR=$(Join-Path $OcctRoot 'win64\vc14\lib')",
    "-DBOOST_ROOT=$BoostPrefix", "-DBoost_USE_STATIC_LIBS=ON", "-DBoost_NO_SYSTEM_PATHS=ON",
    "-DEIGEN_DIR=$(Join-Path $WorkDir 'src\eigen-3.4.1')") (Join-Path $WorkDir "ifcopenshell-configure.log")
Step "IfcOpenShell 0.9.0post1 (compilation, $Jobs tâches)"
Invoke-Native "cmake" @("--build", (Join-Path $WorkDir "build-ifcopenshell"), "--parallel", "$Jobs") (Join-Path $WorkDir "ifcopenshell-build.log")
Invoke-Native "cmake" @("--install", (Join-Path $WorkDir "build-ifcopenshell")) (Join-Path $WorkDir "ifcopenshell-install.log")

# --- 5. Textes de licence livrés avec les DLL (repris par cmake/PackageTSA.cmake) ---------------------------------
# IfcOpenShell : LGPL-3.0 (COPYING.LESSER) qui complète la GPL-3.0 (COPYING) ; Boost : BSL-1.0 ; Eigen : MPL-2.0
# (COPYING.README : répartition des licences des fichiers d'Eigen).
$licenses = @(
    @{ From = (Join-Path $IfcSrc "COPYING"); To = "IfcOpenShell" },
    @{ From = (Join-Path $IfcSrc "COPYING.LESSER"); To = "IfcOpenShell" },
    @{ From = (Join-Path $WorkDir "src\boost-1.92.0\LICENSE_1_0.txt"); To = "Boost" },
    @{ From = (Join-Path $WorkDir "src\eigen-3.4.1\COPYING.MPL2"); To = "Eigen" },
    @{ From = (Join-Path $WorkDir "src\eigen-3.4.1\COPYING.README"); To = "Eigen" }
)
foreach ($l in $licenses) {
    if (-not (Test-Path $l.From)) { throw "Texte de licence introuvable : $($l.From)" }
    $dest = Join-Path $IfcPrefix ("licenses\" + $l.To)
    New-Item -ItemType Directory -Force $dest | Out-Null
    Copy-Item $l.From $dest -Force
}

# --- 6. Trace de la compilation -----------------------------------------------------------------------------------
$stamp = [ordered]@{
    ifcopenshell = "0.9.0post1"; boost = "1.92.0"; eigen = "3.4.1"
    # cl.exe écrit sa bannière sur stderr : redirigée par cmd (PowerShell 5.1 la rendrait fatale sous Stop).
    occt = (Split-Path $OcctRoot -Leaf); compiler = (cmd /c "cl.exe 2>&1" | Select-Object -First 1)
    sha256 = @{}; date = (Get-Date -Format "yyyy-MM-ddTHH:mm:ss")
}
foreach ($s in $Sources) { $stamp.sha256[$s.File] = $s.Sha256 }
$stamp | ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $IfcPrefix "TSA-BUILD.json")
Step "Terminé : $IfcPrefix"
Get-ChildItem (Join-Path $IfcPrefix "bin") -Filter *.dll | ForEach-Object { "  " + $_.Name }
