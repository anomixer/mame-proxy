@echo off
setlocal

echo ==========================================
echo MameProxy Build Script
echo ==========================================

:: 1. Check for WinFsp
:: We use goto to avoid parenthesis parsing issues with "Program Files (x86)" inside an if-block.
set "WINFSP_INC=C:\Program Files (x86)\WinFsp\inc\winfsp\winfsp.h"

if exist "%WINFSP_INC%" goto found_winfsp

echo [ERROR] WinFsp headers not found at:
echo %WINFSP_INC%
echo.
echo Please install WinFsp from https://winfsp.dev/
echo IMPORTANT: Make sure to install "Core" and "Developer" components.
echo.
pause
exit /b 1

:found_winfsp

:: 2. Check CMake
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found in PATH.
    echo Please open this script from a "Visual Studio Developer Command Prompt"
    echo or ensure CMake is installed and in your PATH.
    pause
    exit /b 1
)

:: 3. Create build directory
if not exist build (
    echo [INFO] Creating build directory...
    mkdir build
)

:: 4. Run CMake Generation
cd build
echo [INFO] Running CMake configuration...
cmake ..
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed.
    echo Please ensure Visual Studio 2022 C++ tools are installed.
    cd ..
    pause
    exit /b 1
)

:: 5. Run Build
echo [INFO] Building Release configuration...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed.
    cd ..
    pause
    exit /b 1
)

echo.
echo ==========================================
echo [SUCCESS] Build completed successfully!
echo Executable location: build\Release\MameProxy.exe
echo ==========================================
echo.
cd ..
pause
