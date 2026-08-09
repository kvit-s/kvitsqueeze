@echo off
rem Stage the Qt runtime and the audio engine next to the built sqeezeamp.exe
rem so it runs from Explorer. Also the basis of the portable zip.
rem
rem --qmldir qml is what makes windeployqt deploy the QML modules the shell
rem imports. The app compiles its QML into resources.qrc, so without a
rem directory to scan the deploy tool has no import graph to follow and ships
rem a binary that cannot load its own UI.
setlocal enabledelayedexpansion
if not defined QT_ROOT_DIR set QT_ROOT_DIR=C:\Qt\6.10.1\msvc2022_64
cd /d %~dp0

set STAGE=build-windows-msvc-release\Release

"%QT_ROOT_DIR%\bin\windeployqt.exe" --release --qmldir qml %STAGE%\sqeezeamp.exe > windeploy.log 2>&1
if errorlevel 1 (
    echo win-deploy: windeployqt failed, see windeploy.log
    exit /b 1
)

rem ── The MSVC C runtime.
rem
rem windeployqt stages Qt and stops there. Without this the tree runs only on a
rem machine that already has the runtime — which every build machine does, and a
rem fresh one does not.
powershell -NoProfile -ExecutionPolicy Bypass -File packaging\windows\stage-crt.ps1 -StageDir "%STAGE%"
if errorlevel 1 (
    echo win-deploy: could not stage the MSVC C runtime
    exit /b 1
)

rem ── The audio engine (prd.md §7.3.2).
rem
rem Stock squeezelite.exe, unmodified, launched as a child process. It is not
rem committed to the tree — pinned by version and checksum in
rem packaging\engine-version.txt and staged from wherever it was downloaded.
if not defined SQZ_ENGINE_EXE set SQZ_ENGINE_EXE=packaging\windows\squeezelite.exe
if exist "%SQZ_ENGINE_EXE%" (
    if not exist "%STAGE%\engine" mkdir "%STAGE%\engine"
    copy /y "%SQZ_ENGINE_EXE%" "%STAGE%\engine\squeezelite.exe" >nul
    echo win-deploy: staged the audio engine from %SQZ_ENGINE_EXE%
) else (
    echo win-deploy: no audio engine at %SQZ_ENGINE_EXE% — the app will run
    echo win-deploy: but will not play. See packaging\engine-version.txt.
)

rem ── The licence payload (prd.md §11.2).
rem
rem Shipping the GPLv3 engine binary obliges the package to carry its licence
rem text and a written offer for its source. These travel with every artifact,
rem which is why CMakeLists.txt refuses to configure without them.
if not exist "%STAGE%\licenses" mkdir "%STAGE%\licenses"
copy /y THIRD-PARTY-NOTICES.md "%STAGE%\licenses\" >nul
copy /y packaging\licenses\LICENSE.squeezelite "%STAGE%\licenses\" >nul
copy /y packaging\licenses\WRITTEN-OFFER.txt "%STAGE%\licenses\" >nul
copy /y packaging\engine-version.txt "%STAGE%\licenses\" >nul

echo win-deploy: staged into %STAGE%
exit /b 0
