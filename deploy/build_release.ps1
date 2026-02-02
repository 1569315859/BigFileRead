# ============================================================================
# BigFileViewer Full Build & Package Script
# ============================================================================
# This script performs a complete build and packaging workflow:
#   1. Configure CMake
#   2. Build Release version
#   3. Run windeployqt
#   4. Create NSIS installer (optional)
#   5. Create portable ZIP
#
# Usage:
#   .\build_release.ps1
#   .\build_release.ps1 -SkipInstaller
#   .\build_release.ps1 -Version "1.2.0"
# ============================================================================

param(
    [string]$Version = "1.0.0",
    [switch]$SkipInstaller = $false,
    [switch]$SkipZip = $false,
    [string]$QtDir = "E:\QT\6.10.1\msvc2022_64",
    [string]$VSPath = "E:\Program Files\VS2026"
)

$ErrorActionPreference = "Stop"

# ============================================================================
# Configuration
# ============================================================================
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build\windows-release"
$BinDir = Join-Path $ProjectRoot "bin"
$DeployDir = Join-Path $PSScriptRoot "Release"
$AppName = "BigFileViewer"

Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host " BigFileViewer Build & Package Script" -ForegroundColor Cyan
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Version:     $Version"
Write-Host "Qt Dir:      $QtDir"
Write-Host "Project:     $ProjectRoot"
Write-Host "Build Dir:   $BuildDir"
Write-Host "Deploy Dir:  $DeployDir"
Write-Host ""

# ============================================================================
# Step 1: Initialize VS Developer Environment
# ============================================================================
Write-Host "[1/6] Initializing Visual Studio environment..." -ForegroundColor Yellow

$VsDevShell = Join-Path $VSPath "Common7\Tools\Launch-VsDevShell.ps1"
if (Test-Path $VsDevShell) {
    & $VsDevShell -Arch amd64 -SkipAutomaticLocation
} else {
    Write-Warning "VS Developer Shell not found at: $VsDevShell"
    Write-Host "Attempting to continue without VS environment..."
}

# ============================================================================
# Step 2: Configure CMake
# ============================================================================
Write-Host "[2/6] Configuring CMake..." -ForegroundColor Yellow

Push-Location $ProjectRoot
try {
    $CmakePath = Join-Path $VSPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (-not (Test-Path $CmakePath)) {
        $CmakePath = "cmake"  # Fall back to PATH
    }
    
    & $CmakePath --preset release
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }
} finally {
    Pop-Location
}

# ============================================================================
# Step 3: Build Project
# ============================================================================
Write-Host "[3/6] Building project..." -ForegroundColor Yellow

Push-Location $ProjectRoot
try {
    & $CmakePath --build --preset build-release
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }
} finally {
    Pop-Location
}

# ============================================================================
# Step 4: Run windeployqt
# ============================================================================
Write-Host "[4/6] Running windeployqt..." -ForegroundColor Yellow

# Clean deploy directory
if (Test-Path $DeployDir) {
    Remove-Item $DeployDir -Recurse -Force
}
New-Item -ItemType Directory -Path $DeployDir | Out-Null

# Copy executable
Copy-Item (Join-Path $BinDir "$AppName.exe") $DeployDir

# Copy translations if exist
$TranslationsDir = Join-Path $BinDir "translations"
if (Test-Path $TranslationsDir) {
    $DeployTranslations = Join-Path $DeployDir "translations"
    New-Item -ItemType Directory -Path $DeployTranslations -Force | Out-Null
    Copy-Item (Join-Path $TranslationsDir "*.qm") $DeployTranslations -ErrorAction SilentlyContinue
}

# Run windeployqt
$WinDeployQt = Join-Path $QtDir "bin\windeployqt.exe"
& $WinDeployQt `
    --release `
    --no-translations `
    --no-system-d3d-compiler `
    --no-opengl-sw `
    --compiler-runtime `
    (Join-Path $DeployDir "$AppName.exe")

if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed"
}

# Create version file
@"
$AppName Windows Release
Version: $Version
Build Date: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
Qt Version: 6.10.1
"@ | Out-File (Join-Path $DeployDir "VERSION.txt") -Encoding UTF8

Write-Host "      windeployqt completed successfully" -ForegroundColor Green

# ============================================================================
# Step 5: Create NSIS Installer (optional)
# ============================================================================
if (-not $SkipInstaller) {
    Write-Host "[5/6] Creating NSIS installer..." -ForegroundColor Yellow
    
    $MakeNsis = "C:\Program Files (x86)\NSIS\makensis.exe"
    if (Test-Path $MakeNsis) {
        # Update version in NSI script
        $NsiFile = Join-Path $PSScriptRoot "installer.nsi"
        $NsiContent = Get-Content $NsiFile -Raw
        $NsiContent = $NsiContent -replace '!define APP_VERSION "[^"]*"', "!define APP_VERSION `"$Version`""
        $NsiContent | Set-Content $NsiFile -NoNewline
        
        Push-Location $PSScriptRoot
        try {
            & $MakeNsis installer.nsi
            if ($LASTEXITCODE -ne 0) {
                Write-Warning "NSIS installer creation failed"
            } else {
                $InstallerPath = Join-Path $PSScriptRoot "$AppName-$Version-Setup.exe"
                Write-Host "      Installer created: $InstallerPath" -ForegroundColor Green
            }
        } finally {
            Pop-Location
        }
    } else {
        Write-Warning "NSIS not found at: $MakeNsis (skipping installer)"
    }
} else {
    Write-Host "[5/6] Skipping NSIS installer (--SkipInstaller)" -ForegroundColor DarkGray
}

# ============================================================================
# Step 6: Create Portable ZIP
# ============================================================================
if (-not $SkipZip) {
    Write-Host "[6/6] Creating portable ZIP..." -ForegroundColor Yellow
    
    $ZipPath = Join-Path $PSScriptRoot "$AppName-$Version-Portable.zip"
    if (Test-Path $ZipPath) {
        Remove-Item $ZipPath -Force
    }
    
    Compress-Archive -Path "$DeployDir\*" -DestinationPath $ZipPath -CompressionLevel Optimal
    
    $ZipSize = [math]::Round((Get-Item $ZipPath).Length / 1MB, 2)
    Write-Host "      Portable ZIP created: $ZipPath ($ZipSize MB)" -ForegroundColor Green
} else {
    Write-Host "[6/6] Skipping portable ZIP (--SkipZip)" -ForegroundColor DarkGray
}

# ============================================================================
# Done!
# ============================================================================
Write-Host ""
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host " Build & Package Complete!" -ForegroundColor Cyan
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Output files:"

$OutputFiles = @()
if (Test-Path (Join-Path $PSScriptRoot "$AppName-$Version-Setup.exe")) {
    $OutputFiles += "  - $AppName-$Version-Setup.exe (NSIS Installer)"
}
if (Test-Path (Join-Path $PSScriptRoot "$AppName-$Version-Portable.zip")) {
    $OutputFiles += "  - $AppName-$Version-Portable.zip (Portable)"
}
$OutputFiles += "  - Release\ folder (Unpacked files)"

$OutputFiles | ForEach-Object { Write-Host $_ -ForegroundColor Green }

Write-Host ""
Write-Host "============================================================================" -ForegroundColor Cyan
