@echo off
setlocal
set "PROJECT_ROOT=%~dp0.."
cd /d "%PROJECT_ROOT%"

set "CONFIG=Release"
if "%~1"=="Debug" set "CONFIG=Debug"
if "%~1"=="debug" set "CONFIG=Debug"

if exist "build\%CONFIG%\AquaShell.exe" (
    echo Launching AquaShell (%CONFIG%)...
    start "" "build\%CONFIG%\AquaShell.exe"
) else (
    echo [INFO] AquaShell.exe not found for %CONFIG%. Invoking build script...
    call "%~dp0build.bat" %CONFIG%
    if exist "build\%CONFIG%\AquaShell.exe" (
        start "" "build\%CONFIG%\AquaShell.exe"
    ) else (
        echo [ERROR] Build completed but executable not found.
    )
)
