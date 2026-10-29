@echo off
setlocal EnableExtensions

:: Usage : run.bat [dossier_de_build | chemin\TSA.exe]
::   run.bat                           -> lance le TSA.exe le plus recemment compile parmi build*\ et
::                                        build*\{Release,Debug,RelWithDebInfo,MinSizeRel}\
::   run.bat build-windows-validation  -> lance le TSA.exe de ce dossier (relatif a la racine du projet ou absolu)
::   run.bat build\Release             -> idem (presets Visual Studio)
::
:: Un build deploye (Qt6Core.dll a cote de TSA.exe : windeployqt + DLL OCCT et tierces) se lance tel quel, sans
:: toucher au PATH. Sinon, Qt et OCCT sont ceux enregistres par CMake pour ce build (CMakeCache.txt), puis, a defaut,
:: la Qt 6 la plus recente de C:\Qt et l'OCCT du depot : jamais une version figee dans ce script.

set "TSA_DIR=%~dp0"
set "TSA_EXE="

if not "%~1"=="" goto explicit

:: Un seul appel PowerShell : TSA.exe le plus recent parmi tous les dossiers de build
for /f "usebackq delims=" %%I in (`powershell -NoProfile -NoLogo -Command "$r=$env:TSA_DIR; Get-ChildItem -LiteralPath $r -Directory -Filter 'build*' | ForEach-Object { $_.FullName; Get-ChildItem -LiteralPath $_.FullName -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -in 'Release','Debug','RelWithDebInfo','MinSizeRel' } | ForEach-Object { $_.FullName } } | ForEach-Object { Join-Path $_ 'TSA.exe' } | Where-Object { Test-Path -LiteralPath $_ } | Get-Item | Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName"`) do set "TSA_EXE=%%I"
goto found

:explicit
:: Dossier relatif a la racine du projet, dossier absolu (ou relatif au dossier courant), ou chemin de TSA.exe
if exist "%TSA_DIR%%~1\TSA.exe" set "TSA_EXE=%TSA_DIR%%~1\TSA.exe" & goto found
if exist "%~f1\TSA.exe" set "TSA_EXE=%~f1\TSA.exe" & goto found
if /i "%~nx1"=="TSA.exe" if exist "%~f1" set "TSA_EXE=%~f1"

:found
if not defined TSA_EXE goto notfound
for %%F in ("%TSA_EXE%") do set "BUILD_DIR=%%~dpF"
set "BUILD_DIR=%BUILD_DIR:~0,-1%"

if exist "%BUILD_DIR%\Qt6Core.dll" goto launch
if exist "%BUILD_DIR%\Qt6Cored.dll" goto launch

:: --- Build non deploye : environnement Qt / OCCT ---------------------------------------------------------------------
:: CMakeCache.txt du build (generateur multi-configuration : dossier parent de Release\, Debug\...)
set "CACHE=%BUILD_DIR%\CMakeCache.txt"
if not exist "%CACHE%" for %%P in ("%BUILD_DIR%\..") do set "CACHE=%%~fP\CMakeCache.txt"

set "QT_DIR="
set "OCCT_DIR="
if exist "%CACHE%" (
    for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"Qt6_DIR:" "%CACHE%"') do set "QT_DIR=%%B"
    for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"OpenCASCADE_DIR:" "%CACHE%"') do set "OCCT_DIR=%%B"
)
:: Qt6_DIR = <racine>/lib/cmake/Qt6 ; OpenCASCADE_DIR = <racine>/cmake
if defined QT_DIR set "QT_DIR=%QT_DIR:/lib/cmake/Qt6=%"
if defined QT_DIR set "QT_DIR=%QT_DIR:/=\%"
if defined OCCT_DIR set "OCCT_DIR=%OCCT_DIR:/cmake=%"
if defined OCCT_DIR set "OCCT_DIR=%OCCT_DIR:/=\%"

if defined QT_DIR if not exist "%QT_DIR%\bin\Qt6Core.dll" if not exist "%QT_DIR%\bin\Qt6Cored.dll" set "QT_DIR="
if not defined QT_DIR (
    for /f "usebackq delims=" %%Q in (`powershell -NoProfile -NoLogo -Command "Get-ChildItem 'C:\Qt' -Directory -Filter '6.*' -ErrorAction SilentlyContinue | Sort-Object { try { [version]$_.Name } catch { [version]'0.0' } } -Descending | ForEach-Object { Get-ChildItem $_.FullName -Directory -Filter 'msvc*_64' } | Where-Object { Test-Path (Join-Path $_.FullName 'bin\Qt6Core.dll') } | Select-Object -First 1 -ExpandProperty FullName"`) do set "QT_DIR=%%Q"
)
if not defined OCCT_DIR set "OCCT_DIR=%TSA_DIR%opencascade-8.0.1-vc14-64"
set "TP_DIR=%TSA_DIR%3rdparty-vc14-64"

if not defined QT_DIR (
    echo [AVERTISSEMENT] Build non deploye et aucune Qt 6 MSVC trouvee ^(CMakeCache.txt, C:\Qt^) : TSA risque de ne pas demarrer.
) else (
    set "PATH=%QT_DIR%\bin;%PATH%"
    set "QT_PLUGIN_PATH=%QT_DIR%\plugins"
)
if exist "%OCCT_DIR%\win64\vc14\bin" set "PATH=%OCCT_DIR%\win64\vc14\bin;%PATH%"
if exist "%OCCT_DIR%\src" set "CSF_OCCTResourcePath=%OCCT_DIR%\src"
if exist "%OCCT_DIR%\src\Shaders" set "CSF_OCCTShadersPath=%OCCT_DIR%\src\Shaders"
for %%D in (freetype-2.13.3-x64\bin tbb-2021.13.0-x64\bin jemalloc-vc14-64\bin freeimage-3.18.0-x64\bin openvr-1.14.15-64\bin\win64 ffmpeg-3.3.4-64\bin tcltk-8.6.15-x64\bin) do (
    if exist "%TP_DIR%\%%D" call set "PATH=%TP_DIR%\%%D;%%PATH%%"
)
echo Environnement : Qt [%QT_DIR%], OCCT [%OCCT_DIR%]

:launch
echo Lancement de TSA [%TSA_EXE%]...
:: Dossier de travail = dossier du build : aucun fichier ecrit a la racine du depot
start "" /D "%BUILD_DIR%" "%TSA_EXE%"
endlocal
exit /b 0

:notfound
echo [ERREUR] TSA.exe introuvable.
if "%~1"=="" (
    echo Dossiers recherches : build*\ et build*\{Release,Debug,RelWithDebInfo,MinSizeRel}\ sous
    echo     %TSA_DIR%
) else (
    echo Aucun TSA.exe dans "%~1" ^(ni sous %TSA_DIR%%~1^).
)
echo Compiler le projet, par exemple :
echo     cmake --preset ninja-release ^&^& cmake --build --preset ninja-release
echo ou indiquer le dossier du build : run.bat build-windows-validation
pause
endlocal
exit /b 1
