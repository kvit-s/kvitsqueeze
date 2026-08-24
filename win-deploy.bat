@echo off
rem Stage the Qt runtime and the audio engine next to the built kvitsqueeze.exe
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

"%QT_ROOT_DIR%\bin\windeployqt.exe" --release --qmldir qml %STAGE%\kvitsqueeze.exe > windeploy.log 2>&1
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

rem ── The audio engine (prd.md §7.3.2) — development only.
rem
rem Stock squeezelite.exe, unmodified, launched as a child process. KvitSqueeze
rem does not distribute it: the installer downloads it during setup and
rem win-package.bat keeps engine\ out of both shipped artifacts. This staging
rem exists only so the tree runs from Explorer on the machine that built it.
rem
rem To get one, run packaging\windows\fetch-engine.ps1, or point SQZ_ENGINE_EXE
rem at a copy you already have. The pinned build is in packaging\engine-version.txt.
if not defined SQZ_ENGINE_EXE set SQZ_ENGINE_EXE=packaging\windows\squeezelite.exe
if exist "%SQZ_ENGINE_EXE%" (
    if not exist "%STAGE%\engine" mkdir "%STAGE%\engine"
    copy /y "%SQZ_ENGINE_EXE%" "%STAGE%\engine\squeezelite.exe" >nul
    echo win-deploy: staged the audio engine from %SQZ_ENGINE_EXE%
) else (
    echo win-deploy: no audio engine at %SQZ_ENGINE_EXE% — the app will run
    echo win-deploy: but will not play. Run packaging\windows\fetch-engine.ps1.
)

rem ── The engine fetcher, which does ship.
rem
rem The portable zip has no installer to download the engine, so it carries the
rem script that does — plus a local copy of the manifest, so an unreachable
rem GitHub degrades to "the pinned URL" instead of "no idea where to look".
rem The same script repairs an install whose engine went missing.
copy /y packaging\windows\fetch-engine.ps1 "%STAGE%\" >nul
copy /y packaging\engine-manifest.txt "%STAGE%\" >nul

rem ── The licence payload (prd.md §11.2).
rem
rem KvitSqueeze's own MPL-2.0 obliges every artifact to carry LICENSE, which is
rem why CMakeLists.txt refuses to configure without it. The GPLv3 text travels
rem too, even though the engine binary no longer does: a user whose installer
rem fetched squeezelite has a GPLv3 program on disk and should have its terms.
rem See THIRD-PARTY-NOTICES.md for why fetching is not distributing.
if not exist "%STAGE%\licenses" mkdir "%STAGE%\licenses"
copy /y LICENSE "%STAGE%\licenses\" >nul
copy /y THIRD-PARTY-NOTICES.md "%STAGE%\licenses\" >nul
copy /y packaging\licenses\LICENSE.squeezelite "%STAGE%\licenses\" >nul
copy /y packaging\engine-version.txt "%STAGE%\licenses\" >nul

echo win-deploy: staged into %STAGE%
exit /b 0
