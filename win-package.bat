@echo off
rem Build the shippable artifacts (prd.md M5, §13 Q5).
rem
rem   win-package.bat        portable zip, and the installer if Inno Setup is
rem                          installed
rem   win-package.bat zip    portable zip only
rem
rem Both are built from the tree win-deploy.bat staged, so what ships is
rem exactly what was run and tested from that directory — the same executable,
rem the same Qt DLLs, the same licence payload. The audio engine is the one
rem thing that does not ship; it is downloaded at install time instead, and
rem removed from the packaged tree below.
rem
rem prd.md NFR-8: one documented command produces the installer. This is it.
setlocal enabledelayedexpansion
cd /d %~dp0

set STAGE=build-windows-msvc-release\Release
set DIST=dist

rem The version comes from CMakeLists.txt, which is where the binary gets it
rem too — a second copy here would eventually name an artifact after a version
rem the executable inside it does not report. SQZ_VERSION_FULL overrides it for
rem a prerelease build, exactly as it does for the CMake configure.
if defined SQZ_VERSION_FULL (
    set VERSION=%SQZ_VERSION_FULL%
) else (
    for /f "tokens=3" %%v in ('findstr /r /c:"^project(sqeezeamp VERSION " CMakeLists.txt') do set VERSION=%%v
)
if not defined VERSION (
    echo win-package: could not read the version from CMakeLists.txt
    exit /b 1
)

rem Inno Setup installs per-user by default on a machine where the installer was
rem not run elevated, so both locations are tried before giving up.
set ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe
if not exist "%ISCC%" set ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe

if not exist "%STAGE%\sqeezeamp.exe" (
    echo win-package: nothing staged. Run win-build.bat then win-deploy.bat first.
    exit /b 1
)

rem The licence payload is a distribution obligation, not a nicety: MPL-2.0
rem obliges every artifact to carry LICENSE, and the notices name what the
rem installer will go and fetch (prd.md §11.2).
if not exist "%STAGE%\licenses\LICENSE.squeezelite" (
    echo win-package: the licence payload is missing from %STAGE%\licenses.
    echo win-package: run win-deploy.bat, which stages it.
    exit /b 1
)
rem The engine is deliberately absent from both artifacts, so what has to be
rem present instead is the script that fetches it. A portable zip without it is
rem a dead end for whoever unpacks it.
if not exist "%STAGE%\fetch-engine.ps1" (
    echo win-package: fetch-engine.ps1 is missing from %STAGE%.
    echo win-package: run win-deploy.bat, which stages it.
    exit /b 1
)

if not exist "%DIST%" mkdir "%DIST%"

rem ── The tree both artifacts are built from.
rem
rem A copy of the staged tree under its release name, so the zip unpacks into
rem one folder instead of emptying two hundred files into whatever directory it
rem was opened in. The installer is compiled from this same copy, which is what
rem makes "the zip and the setup contain the same bytes" a fact rather than an
rem intention. robocopy reports success as any exit code below 8.
set PKGNAME=SqeezeAmp-%VERSION%-windows-x64
set PKGDIR=build-windows-msvc-release\%PKGNAME%
robocopy "%STAGE%" "%PKGDIR%" /MIR /NJH /NJS /NFL /NDL >nul
if errorlevel 8 (
    echo win-package: could not assemble %PKGDIR%
    exit /b 1
)

rem squeezelite is GPLv3 and is not distributed (THIRD-PARTY-NOTICES.md). A
rem development tree has one staged so the app runs from Explorer; neither the
rem zip nor the installer may carry it. Removed after the mirror rather than
rem excluded from it, so a copy left by an earlier run goes too.
if exist "%PKGDIR%\engine" rmdir /s /q "%PKGDIR%\engine"

rem ── Portable zip.
rem
rem Compress-Archive opens every staged file for read, and a DLL written moments
rem ago can still be held briefly by real-time antivirus or the search indexer.
rem That surfaces as an IOException part-way through the archive and clears in
rem seconds, so it is retried rather than treated as a failure.
set ZIP=%DIST%\%PKGNAME%.zip
if exist "%ZIP%" del "%ZIP%"
powershell -NoProfile -Command "$ErrorActionPreference='Stop'; for ($i=1; $i -le 5; $i++) { try { Compress-Archive -Path '%PKGDIR%' -DestinationPath '%ZIP%' -CompressionLevel Optimal -Force; break } catch { if ($i -eq 5) { throw }; Write-Host ('win-package: zip attempt ' + $i + ' failed, retrying'); Start-Sleep -Seconds 3 } }"
if errorlevel 1 (
    echo win-package: could not build the portable zip
    exit /b 1
)
echo win-package: %ZIP%

if "%1"=="zip" (
    call :checksums
    exit /b 0
)

rem ── Installer.
if not exist "%ISCC%" (
    echo win-package: Inno Setup 6 not found at "%ISCC%"
    echo win-package: portable zip only. Set ISCC in this script to build the installer.
    exit /b 0
)

"%ISCC%" /DAppVersion=%VERSION% "/DStageDir=%CD%\%PKGDIR%" packaging\windows\sqeezeamp.iss
if errorlevel 1 (
    echo win-package: Inno Setup failed
    exit /b 1
)
echo win-package: %DIST%\SqeezeAmp-%VERSION%-setup.exe

call :checksums
exit /b 0

rem ── Checksums for everything in dist.
:checksums
powershell -NoProfile -ExecutionPolicy Bypass -File packaging\windows\checksums.ps1 -DistDir "%DIST%"
if errorlevel 1 echo win-package: could not write the checksums
goto :eof
