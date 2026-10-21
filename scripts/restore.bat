@echo off
setlocal
set "PROJECT_ROOT=%~dp0.."
cd /d "%PROJECT_ROOT%"

echo Restoring Windows Taskbar and Work Area...
taskkill /F /IM AquaShell.exe >nul 2>&1

if exist "RestoreTaskbar.exe" (
    "RestoreTaskbar.exe" >nul 2>&1
) else if exist "build\Release\RestoreTaskbar.exe" (
    "build\Release\RestoreTaskbar.exe" >nul 2>&1
) else if exist "build\Debug\RestoreTaskbar.exe" (
    "build\Debug\RestoreTaskbar.exe" >nul 2>&1
)

if exist "%~dp0restore.ps1" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0restore.ps1" >nul 2>&1
)

echo Taskbar and system desktop restored successfully!
exit /b 0
