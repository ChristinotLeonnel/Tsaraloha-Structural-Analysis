@echo off
setlocal

set "TSA_DIR=%~dp0"
set "QT_DIR=C:\Qt\6.11.2\msvc2022_64"
set "OCCT_DIR=%TSA_DIR%opencascade-8.0.1-vc14-64"
set "TP_DIR=%TSA_DIR%3rdparty-vc14-64"

:: Configuration des variables OCCT pour les shaders et ressources
set "CSF_OCCTResourcePath=%OCCT_DIR%\src"
set "CSF_OCCTShadersPath=%OCCT_DIR%\src\OpenGl"
set "QT_PLUGIN_PATH=%QT_DIR%\plugins"

:: Ajout de toutes les dépendances OCCT, 3rdparty et Qt au PATH
set "PATH=%QT_DIR%\bin;%OCCT_DIR%\win64\vc14\bin;%TP_DIR%\freetype-2.13.3-x64\bin;%TP_DIR%\tbb-2021.13.0-x64\bin;%TP_DIR%\jemalloc-vc14-64\bin;%TP_DIR%\freeimage-3.18.0-x64\bin;%TP_DIR%\openvr-1.14.15-64\bin\win64;%TP_DIR%\openvr-1.14.15-64\bin;%TP_DIR%\ffmpeg-3.3.4-64\bin;%TP_DIR%\tcltk-8.6.15-x64\bin;%PATH%"

if exist "%TSA_DIR%build\Release\TSA.exe" goto launch_release
if exist "%TSA_DIR%build\Debug\TSA.exe" goto launch_debug

echo [ERREUR] TSA.exe introuvable dans build\Release ou build\Debug.
echo Veuillez compiler le projet avant de lancer le script.
pause
exit /b 1

:launch_release
echo Lancement de TSA [Release]...
start "" "%TSA_DIR%build\Release\TSA.exe"
goto end

:launch_debug
echo Lancement de TSA [Debug]...
start "" "%TSA_DIR%build\Debug\TSA.exe"
goto end

:end
endlocal
