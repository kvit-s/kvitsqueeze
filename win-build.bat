@echo off
rem Configure + build SqeezeAmp with MSVC 2022 / Qt 6.10.1.
rem Usage: win-build.bat [configure|build|test]  (no arg = configure+build)
rem
rem Unlike kvit-notes there is no WSL mirror here: this tree IS the checkout,
rem edited in place, so there is nothing to sync before a build.
rem
rem Toolchain paths can be overridden in the environment; the defaults match a
rem stock Visual Studio 2022 Community plus Qt under C:\Qt.
setlocal enabledelayedexpansion
if not defined QT_ROOT_DIR set QT_ROOT_DIR=C:\Qt\6.10.1\msvc2022_64
if not defined VS_CMAKE_DIR set VS_CMAKE_DIR=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin
set CMAKE=%VS_CMAKE_DIR%\cmake.exe
set CTEST=%VS_CMAKE_DIR%\ctest.exe
cd /d %~dp0

if not exist "%QT_ROOT_DIR%\bin\qmake.exe" (
    echo win-build: no Qt kit at %QT_ROOT_DIR%
    echo win-build: set QT_ROOT_DIR to the msvc2022_64 kit you want to build against.
    exit /b 1
)

if "%1"=="build" goto build
if "%1"=="test" goto test

"%CMAKE%" --preset windows-msvc-release
if errorlevel 1 exit /b 1
if "%1"=="configure" exit /b 0

:build
rem Close the running app before rebuilding: a locked sqeezeamp.exe fails its
rem link with LNK1104 while the rest of the build succeeds, leaving a stale
rem exe that looks fresh.
"%CMAKE%" --build --preset windows-msvc-release -j 8
exit /b %errorlevel%

:test
set PATH=%QT_ROOT_DIR%\bin;%PATH%
"%CTEST%" --test-dir build-windows-msvc-release -C Release -L unit --output-on-failure
exit /b %errorlevel%
