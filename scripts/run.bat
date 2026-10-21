@echo off
setlocal
set "PROJECT_ROOT=%~dp0.."
cd /d "%PROJECT_ROOT%"

set "CONFIG=Release"
if /i "%~1"=="debug" set "CONFIG=Debug"

if not exist "%PROJECT_ROOT%\build\%CONFIG%\AquaShell.exe" goto :do_build

:do_launch
echo Launching AquaShell [%CONFIG%]...
start "" "%PROJECT_ROOT%\build\%CONFIG%\AquaShell.exe"
exit /b 0

:do_build
echo [INFO] AquaShell.exe not found for %CONFIG%. Invoking build script...
call "%~dp0build.bat" %CONFIG%
if exist "%PROJECT_ROOT%\build\%CONFIG%\AquaShell.exe" goto :do_launch
echo [ERROR] Build completed but executable was not found at %PROJECT_ROOT%\build\%CONFIG%\AquaShell.exe.
exit /b 1
