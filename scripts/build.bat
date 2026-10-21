@echo off
setlocal

echo ========================================================
echo   Building AquaShell (Enterprise macOS UI for Windows)
echo   Built with C++20 + DirectComposition + Direct2D
echo ========================================================

:: Detect Visual Studio / Build Tools environment
if not defined DevEnvDir (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    )
)

set "PROJECT_ROOT=%~dp0.."
cd /d "%PROJECT_ROOT%"

taskkill /F /IM AquaShell.exe >nul 2>&1

if not exist "build" mkdir "build"
cd "build"

set "CMAKE_CMD=cmake"
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    set "CMAKE_CMD=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)

set "CONFIG=Release"
if /i "%~1"=="debug" set "CONFIG=Debug"

echo.
echo [1/2] Configuring CMake for %CONFIG% (x64)...
"%CMAKE_CMD%" .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed!
    exit /b %ERRORLEVEL%
)

echo.
echo [2/2] Building target AquaShell (%CONFIG%)...
"%CMAKE_CMD%" --build . --config %CONFIG% --parallel
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo   Build Successful! Executable: build\%CONFIG%\AquaShell.exe
echo ========================================================
exit /b 0
