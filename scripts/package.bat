@echo off
setlocal EnableDelayedExpansion

echo ========================================================
echo   Packaging AquaOS Standalone Release and Installer
echo ========================================================

set "PROJECT_ROOT=%~dp0.."
cd /d "%PROJECT_ROOT%"

set "VERSION=2.0.0"
set "DIST_DIR=%PROJECT_ROOT%\dist"
set "STAGE_DIR=%DIST_DIR%\AquaOS"
set "ZIP_NAME=AquaOS-v%VERSION%-windows-x64.zip"

:: 1. Ensure binaries are built in Release mode
if not exist "build\Release\AquaShell.exe" (
    echo [1/4] Release binary not found. Building now...
    call "%PROJECT_ROOT%\scripts\build.bat" Release
    if errorlevel 1 (
        echo [ERROR] Build failed. Cannot package.
        exit /b 1
    )
) else (
    echo [1/4] Found existing Release binaries.
)

:: 2. Prepare staging folder
echo [2/4] Assembling portable distribution...
if exist "%STAGE_DIR%" rmdir /s /q "%STAGE_DIR%"
mkdir "%STAGE_DIR%"
mkdir "%STAGE_DIR%\scripts"

copy /y "build\Release\AquaShell.exe" "%STAGE_DIR%\" >nul
copy /y "build\Release\RestoreTaskbar.exe" "%STAGE_DIR%\" >nul
copy /y "scripts\restore.bat" "%STAGE_DIR%\scripts\" >nul
copy /y "scripts\restore.ps1" "%STAGE_DIR%\scripts\" >nul
copy /y "scripts\run.bat" "%STAGE_DIR%\scripts\" >nul
if exist "README.md" copy /y "README.md" "%STAGE_DIR%\" >nul

:: 3. Create Portable ZIP
echo [3/4] Generating portable zip archive: dist\%ZIP_NAME%...
if exist "%DIST_DIR%\%ZIP_NAME%" del /f /q "%DIST_DIR%\%ZIP_NAME%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE_DIR%' -DestinationPath '%DIST_DIR%\%ZIP_NAME%' -Force"

if exist "%DIST_DIR%\%ZIP_NAME%" (
    echo [OK] Created %DIST_DIR%\%ZIP_NAME%
) else (
    echo [WARNING] Failed to create zip package.
)

:: 4. Build Standalone Inno Setup Installer (if ISCC available)
echo [4/4] Checking for Inno Setup Compiler ISCC...
set "ISCC_PATH="
where ISCC.exe >nul 2>&1
if not errorlevel 1 (
    set "ISCC_PATH=ISCC.exe"
)
if not defined ISCC_PATH (
    if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
        set "ISCC_PATH=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    )
)
if not defined ISCC_PATH (
    if exist "C:\Program Files\Inno Setup 6\ISCC.exe" (
        set "ISCC_PATH=C:\Program Files\Inno Setup 6\ISCC.exe"
    )
)

if defined ISCC_PATH (
    echo Compiling installer with Inno Setup...
    "!ISCC_PATH!" "installer\AquaOS.iss"
    if not errorlevel 1 (
        echo [OK] Installer built successfully: dist\AquaOS-v%VERSION%-Setup.exe
    ) else (
        echo [WARNING] Inno Setup compilation returned an error.
    )
) else (
    echo [INFO] Inno Setup not installed locally. Portable ZIP is ready in dist.
    echo        Note: Inno Setup installer is compiled by GitHub Actions CI/CD pipeline.
)

echo.
echo ========================================================
echo   Packaging Complete!
echo   Outputs available in: %DIST_DIR%
echo ========================================================
exit /b 0
