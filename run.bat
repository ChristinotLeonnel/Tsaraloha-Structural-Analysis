@echo off
setlocal

:: Usage : run.bat [dossier_de_build]
::   run.bat                    -> lance le TSA.exe le plus recemment compile parmi les dossiers connus
::   run.bat build-ninja-debug  -> lance le TSA.exe de ce dossier (chemin relatif a la racine du projet)
::   run.bat build\Release      -> idem (presets Visual Studio)

set "TSA_DIR=%~dp0"
set "QT_DIR=C:\Qt\6.11.2\msvc2022_64"
set "OCCT_DIR=%TSA_DIR%opencascade-8.0.1-vc14-64"
set "TP_DIR=%TSA_DIR%3rdparty-vc14-64"

:: Configuration des variables OCCT pour les shaders et ressources
set "CSF_OCCTResourcePath=%OCCT_DIR%\src"
set "CSF_OCCTShadersPath=%OCCT_DIR%\src\OpenGl"
set "QT_PLUGIN_PATH=%QT_DIR%\plugins"

:: Ajout de toutes les dependances OCCT, 3rdparty et Qt au PATH
set "PATH=%QT_DIR%\bin;%OCCT_DIR%\win64\vc14\bin;%TP_DIR%\freetype-2.13.3-x64\bin;%TP_DIR%\tbb-2021.13.0-x64\bin;%TP_DIR%\jemalloc-vc14-64\bin;%TP_DIR%\freeimage-3.18.0-x64\bin;%TP_DIR%\openvr-1.14.15-64\bin\win64;%TP_DIR%\openvr-1.14.15-64\bin;%TP_DIR%\ffmpeg-3.3.4-64\bin;%TP_DIR%\tcltk-8.6.15-x64\bin;%PATH%"

set "TSA_EXE="

:: Dossier de build explicite passe en argument
if not "%~1"=="" goto explicit

:: Sinon : TSA.exe le plus recent parmi les dossiers de build connus
::   Ninja : build-ninja-release, build-ninja-debug
::   Visual Studio : build-debug\Debug, build\Release, build\Debug
::   Script setup_build.ps1 : build-msvc\Release, build-msvc\Debug
for /f "usebackq delims=" %%I in (`powershell -NoProfile -Command "$d=$env:TSA_DIR; @('build-ninja-release','build-ninja-debug','build-debug\Debug','build\Release','build\Debug','build-msvc\Release','build-msvc\Debug') | ForEach-Object { Join-Path $d ($_+'\TSA.exe') } | Where-Object { Test-Path $_ } | Get-Item | Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName"`) do set "TSA_EXE=%%I"
goto check

:explicit
if exist "%TSA_DIR%%~1\TSA.exe" set "TSA_EXE=%TSA_DIR%%~1\TSA.exe"

:check
if defined TSA_EXE goto launch

echo [ERREUR] TSA.exe introuvable.
echo Dossiers recherches : build-ninja-release, build-ninja-debug, build-debug\Debug,
echo build\Release, build\Debug, build-msvc\Release, build-msvc\Debug.
echo Veuillez compiler le projet avant de lancer le script, ou indiquer le dossier :
echo     run.bat build-ninja-debug
pause
exit /b 1

:launch
echo Lancement de TSA [%TSA_EXE%]...
start "" "%TSA_EXE%"

endlocal
