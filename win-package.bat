@echo off
rem Build the shippable artifacts (prd.md M5, §13 Q5).
rem
rem   win-package.bat        portable zip, and the installer if Inno Setup is
rem                          installed
rem   win-package.bat zip    portable zip only
rem
rem Both are built from the tree win-deploy.bat staged, so what ships is
rem exactly what was run and tested from that directory — the same executable,
rem the same Qt DLLs, the same audio engine, the same licence payload.
rem
rem prd.md NFR-8: one documented command produces the installer. This is it.
setlocal enabledelayedexpansion
cd /d %~dp0

set STAGE=build-windows-msvc-release\Release
set DIST=dist
set VERSION=0.1.0
rem Inno Setup installs per-user by default on a machine where the installer was
rem not run elevated, so both locations are tried before giving up.
set ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe
if not exist "%ISCC%" set ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe

if not exist "%STAGE%\sqeezeamp.exe" (
    echo win-package: nothing staged. Run win-build.bat then win-deploy.bat first.
    exit /b 1
)

rem The licence payload is a distribution obligation, not a nicety: the GPLv3
rem engine binary may not ship without it (prd.md §11.2).
if not exist "%STAGE%\licenses\LICENSE.squeezelite" (
    echo win-package: the licence payload is missing from %STAGE%\licenses.
    echo win-package: run win-deploy.bat, which stages it.
    exit /b 1
)
if not exist "%STAGE%\engine\squeezelite.exe" (
    echo win-package: no audio engine staged — the package would not play.
    echo win-package: see packaging\engine-version.txt.
    exit /b 1
)

if not exist "%DIST%" mkdir "%DIST%"

rem ── Portable zip.
set ZIP=%DIST%\SqeezeAmp-%VERSION%-windows-x64-portable.zip
if exist "%ZIP%" del "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%ZIP%' -CompressionLevel Optimal"
if errorlevel 1 (
    echo win-package: could not build the portable zip
    exit /b 1
)
echo win-package: %ZIP%

if "%1"=="zip" exit /b 0

rem ── Installer.
if not exist "%ISCC%" (
    echo win-package: Inno Setup 6 not found at "%ISCC%"
    echo win-package: portable zip only. Set ISCC in this script to build the installer.
    exit /b 0
)

"%ISCC%" /DAppVersion=%VERSION% "/DStageDir=%CD%\%STAGE%" packaging\windows\sqeezeamp.iss
if errorlevel 1 (
    echo win-package: Inno Setup failed
    exit /b 1
)
echo win-package: %DIST%\SqeezeAmp-%VERSION%-setup.exe
exit /b 0
