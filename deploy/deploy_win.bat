@echo off
REM ============================================================================
REM BigFileViewer Windows Deployment Script
REM ============================================================================
REM This script uses windeployqt to bundle all Qt dependencies for distribution.
REM 
REM Prerequisites:
REM   - Qt installed (with windeployqt in PATH or specified below)
REM   - BigFileViewer.exe already built in Release mode
REM
REM Usage:
REM   1. Open "Qt Command Prompt" or "Developer Command Prompt for VS"
REM   2. Run: deploy_win.bat
REM
REM Output:
REM   - Release\ folder containing BigFileViewer.exe and all dependencies
REM ============================================================================

setlocal enabledelayedexpansion

REM ============================================================================
REM Configuration - Modify these paths according to your environment
REM ============================================================================

REM Qt installation path (adjust to your Qt version)
set QT_DIR=E:\QT\6.10.1\msvc2022_64

REM Build output directory (where BigFileViewer.exe is located)
set BUILD_DIR=%~dp0..\bin

REM Deployment output directory
set DEPLOY_DIR=%~dp0Release

REM Application name
set APP_NAME=BigFileViewer

REM ============================================================================
REM Verify Qt tools exist
REM ============================================================================
if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo [ERROR] windeployqt.exe not found at: %QT_DIR%\bin\windeployqt.exe
    echo Please update QT_DIR in this script to point to your Qt installation.
    exit /b 1
)

REM ============================================================================
REM Verify build output exists
REM ============================================================================
if not exist "%BUILD_DIR%\%APP_NAME%.exe" (
    echo [ERROR] %APP_NAME%.exe not found at: %BUILD_DIR%\%APP_NAME%.exe
    echo Please build the project in Release mode first:
    echo   cmake --preset windows-release
    echo   cmake --build build/windows-release
    exit /b 1
)

echo ============================================================================
echo BigFileViewer Windows Deployment
echo ============================================================================
echo.
echo Qt Directory:    %QT_DIR%
echo Build Directory: %BUILD_DIR%
echo Deploy To:       %DEPLOY_DIR%
echo.

REM ============================================================================
REM Step 1: Create/Clean deployment directory
REM ============================================================================
echo [1/5] Preparing deployment directory...

if exist "%DEPLOY_DIR%" (
    echo      Cleaning existing deployment directory...
    rmdir /s /q "%DEPLOY_DIR%"
)
mkdir "%DEPLOY_DIR%"

REM ============================================================================
REM Step 2: Copy main executable
REM ============================================================================
echo [2/5] Copying executable...

copy /y "%BUILD_DIR%\%APP_NAME%.exe" "%DEPLOY_DIR%\" > nul
if errorlevel 1 (
    echo [ERROR] Failed to copy %APP_NAME%.exe
    exit /b 1
)
echo      Copied: %APP_NAME%.exe

REM ============================================================================
REM Step 3: Copy translation files (if any)
REM ============================================================================
echo [3/5] Copying translation files...

if exist "%BUILD_DIR%\translations" (
    mkdir "%DEPLOY_DIR%\translations" 2>nul
    xcopy /s /y /q "%BUILD_DIR%\translations\*.qm" "%DEPLOY_DIR%\translations\" > nul
    echo      Copied translation files
) else (
    echo      No translation files found (skipping)
)

REM ============================================================================
REM Step 4: Run windeployqt to bundle Qt dependencies
REM ============================================================================
echo [4/5] Running windeployqt to bundle Qt dependencies...

"%QT_DIR%\bin\windeployqt.exe" ^
    --release ^
    --no-translations ^
    --no-system-d3d-compiler ^
    --no-opengl-sw ^
    --compiler-runtime ^
    "%DEPLOY_DIR%\%APP_NAME%.exe"

if errorlevel 1 (
    echo [ERROR] windeployqt failed!
    exit /b 1
)
echo      Qt dependencies bundled successfully

REM ============================================================================
REM Step 5: Create version info file
REM ============================================================================
echo [5/5] Creating version info...

echo BigFileViewer Windows Release > "%DEPLOY_DIR%\VERSION.txt"
echo Build Date: %date% %time% >> "%DEPLOY_DIR%\VERSION.txt"
echo Qt Version: 6.10.1 >> "%DEPLOY_DIR%\VERSION.txt"

REM ============================================================================
REM Done!
REM ============================================================================
echo.
echo ============================================================================
echo Deployment Complete!
echo ============================================================================
echo.
echo Output directory: %DEPLOY_DIR%
echo.
echo Contents:
dir /b "%DEPLOY_DIR%"
echo.
echo ============================================================================
echo Optional: Create Single-File Executable
echo ============================================================================
echo To create a single portable .exe file, you can use:
echo   - Enigma Virtual Box (free): https://enigmaprotector.com/en/downloads.html
echo   - 7-Zip SFX: Package as self-extracting archive
echo   - NSIS: Create proper installer
echo.
echo For Enigma Virtual Box:
echo   1. Download and install Enigma Virtual Box
echo   2. Open it and select %DEPLOY_DIR%\%APP_NAME%.exe as input
echo   3. Add all files from %DEPLOY_DIR% to the virtual file system
echo   4. Click "Process" to create a single portable .exe
echo ============================================================================

endlocal
pause
