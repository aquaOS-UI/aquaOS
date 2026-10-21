@echo off
cd /d "%~dp0"
echo Restoring Windows Taskbar and Work Area...
if exist "RestoreTaskbar.exe" (
    start "" "RestoreTaskbar.exe"
)
echo Done!

