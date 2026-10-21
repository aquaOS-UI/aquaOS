@echo off
cd /d "%~dp0"
if exist "build\Release\AquaShell.exe" (
    echo Launching AquaShell...
    start "" "build\Release\AquaShell.exe"
) else (
    echo AquaShell.exe not found. Running build.bat first...
    call build.bat
    if exist "build\Release\AquaShell.exe" (
        start "" "build\Release\AquaShell.exe"
    )
)

