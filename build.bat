@echo off
setlocal
echo ========================================================
echo   Building AquaShell (macOS UI Layer for Windows)
echo   Built with C++20 + DirectComposition + Direct2D
echo ========================================================

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0"

taskkill /F /IM AquaShell.exe >nul 2>&1

if not exist build mkdir build
cd build

set CMAKE_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

%CMAKE_PATH% .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% neq 0 (
    echo CMake generation failed!
    exit /b %ERRORLEVEL%
)

%CMAKE_PATH% --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo   Build Successful! Executable: build\Release\AquaShell.exe
echo ========================================================

