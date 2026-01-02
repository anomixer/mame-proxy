@echo off
setlocal enabledelayedexpansion

echo ==========================================
echo MameCloudRompath (MCR) Configuration Utility
echo ==========================================

:: 1. Check for WinFsp
set "WINFSP_DLL=C:\Program Files (x86)\WinFsp\bin\winfsp-x64.dll"
if not exist "%WINFSP_DLL%" (
    echo [ERROR] WinFsp not found! 
    echo Please install WinFsp from https://winfsp.dev/
    pause
    exit /b 1
)
echo [OK] WinFsp is installed.

:: 2. Prompt for settings
set /p "MAME_CACHE=Enter MameCache directory (Default: C:\mamecache): "
if "!MAME_CACHE!"=="" set "MAME_CACHE=C:\mamecache"

set /p "DRIVE_LETTER=Enter virtual drive letter (Default: Z:): "
if "!DRIVE_LETTER!"=="" set "DRIVE_LETTER=Z:"

:: Validate Drive Letter format (Ensuring it ends with :)
if not "!DRIVE_LETTER:~-1!"==":" set "DRIVE_LETTER=!DRIVE_LETTER!:"

:: 3. Write to mamefs.ini
echo MOUNT_POINT=!DRIVE_LETTER!> mamefs.ini
echo CACHE_DIR=!MAME_CACHE!>> mamefs.ini
echo BASE_URL=https://mdk.cab/download/>> mamefs.ini

echo [OK] Settings saved to mamefs.ini

:: 4. Check for mcr.exe
if not exist "build\Release\mcr.exe" (
    echo [INFO] mcr.exe not found. Calling build.bat...
    call build.bat
    if errorlevel 1 (
        echo [ERROR] Build failed. Please check the errors above.
        pause
        exit /b 1
    )
)

:: 5. Create mcr.bat
(
    echo @echo off
    echo setlocal enabledelayedexpansion
    echo for /f "tokens=1,2 delims==" %%%%a in (mamefs.ini^) do (
    echo     set %%%%a=%%%%b
    echo ^)
    echo echo Starting MameCloudRompath (MCR) with:
    echo echo Mount Point: %%MOUNT_POINT%%
    echo echo Cache Dir: %%CACHE_DIR%%
    echo echo URL: %%BASE_URL%%
    echo "%%~dp0build\Release\mcr.exe" -m %%MOUNT_POINT%% -c "%%CACHE_DIR%%" -u %%BASE_URL%%
) > mcr.bat

echo ==========================================
echo [SUCCESS] Configuration complete!
echo You can now run the proxy using: mcr.bat
echo ==========================================
pause
