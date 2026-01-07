# BigFileViewer Deployment Guide

This directory contains deployment scripts to package BigFileViewer for distribution on Windows, macOS, and Linux.

## Overview

| Platform | Script | Tool | Output |
|----------|--------|------|--------|
| Windows | `deploy_win.bat` | `windeployqt` | `Release/` folder with all dependencies |
| macOS | `deploy_mac.sh` | `macdeployqt` | `.app` bundle + `.dmg` installer |
| Linux | `deploy_linux.sh` | `linuxdeployqt` | Portable `.AppImage` |

---

## Windows Deployment

### Prerequisites
- Qt installed with MSVC compiler
- BigFileViewer built in Release mode
- Visual Studio runtime (bundled automatically)

### Steps

1. **Build the project in Release mode:**
   ```cmd
   cmake --preset windows-release
   cmake --build build/windows-release
   ```

2. **Run the deployment script:**
   ```cmd
   cd deploy
   deploy_win.bat
   ```

3. **Output:**
   - `deploy/Release/BigFileViewer.exe` - Main executable
   - `deploy/Release/*.dll` - Qt and runtime dependencies
   - `deploy/Release/translations/` - Translation files

### Configuration
Edit `deploy_win.bat` to adjust paths:
```batch
set QT_DIR=E:\QT\6.10.1\msvc2022_64
set BUILD_DIR=%~dp0..\bin
```

### Creating Single Executable (Optional)
Use [Enigma Virtual Box](https://enigmaprotector.com/en/downloads.html) to pack everything into a single `.exe`:
1. Open Enigma Virtual Box
2. Select `Release\BigFileViewer.exe` as input
3. Add all files from `Release\` folder
4. Click "Process" to create portable exe

---

## macOS Deployment

### Prerequisites
- Qt installed (Homebrew or official installer)
- Xcode Command Line Tools
- BigFileViewer built as `.app` bundle

### Steps

1. **Build the project:**
   ```bash
   cmake --preset macos-release
   cmake --build build/macos-release
   ```

2. **Run the deployment script:**
   ```bash
   cd deploy
   chmod +x deploy_mac.sh
   ./deploy_mac.sh
   ```

3. **Output:**
   - `deploy/Release/BigFileViewer.app` - Self-contained app bundle
   - `deploy/Release/BigFileViewer-YYYYMMDD.dmg` - DMG installer image

### Configuration
Edit `deploy_mac.sh` to adjust:
```bash
QT_DIR="/opt/homebrew/opt/qt"  # For Homebrew Qt
CREATE_DMG="yes"                # Set to "no" to skip DMG creation
SIGN_APP="no"                   # Set to "yes" for code signing
```

### Code Signing (Optional)
For distribution outside the App Store:
```bash
SIGN_APP="yes"
DEVELOPER_ID="Developer ID Application: Your Name (XXXXXXXXXX)"
```

### Notarization
For Gatekeeper-friendly distribution:
```bash
xcrun notarytool submit BigFileViewer.dmg \
    --apple-id "your@email.com" \
    --team-id "XXXXXXXXXX" \
    --password "app-specific-password" \
    --wait
```

---

## Linux Deployment

### Prerequisites
- Qt installed
- `linuxdeployqt` (downloaded automatically or manually)
- BigFileViewer built

### Steps

1. **Build the project:**
   ```bash
   cmake --preset linux-release
   cmake --build build/linux-release
   ```

2. **Run the deployment script:**
   ```bash
   cd deploy
   chmod +x deploy_linux.sh
   ./deploy_linux.sh
   ```

3. **Output:**
   - `deploy/Release/BigFileViewer-x86_64.AppImage` - Portable AppImage

### Manual linuxdeployqt Download
If auto-download fails:
```bash
wget https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage
chmod +x linuxdeployqt-continuous-x86_64.AppImage
```

### Running the AppImage
```bash
chmod +x BigFileViewer-x86_64.AppImage
./BigFileViewer-x86_64.AppImage
```

### Desktop Integration
The AppImage can be integrated with the desktop:
```bash
./BigFileViewer-x86_64.AppImage --install
```

---

## Troubleshooting

### Windows: "Qt platform plugin could not be initialized"
- Ensure `platforms/qwindows.dll` exists in the deployment folder
- Run `windeployqt` again

### macOS: "App is damaged and can't be opened"
- The app needs to be signed and notarized, or:
  ```bash
  xattr -cr BigFileViewer.app
  ```

### Linux: "Could not find Qt platform plugin"
- Set `QT_PLUGIN_PATH` environment variable
- Or re-run `linuxdeployqt` with `-bundle-non-qt-libs`

### Missing Translations
- Ensure `.qm` files are in `bin/translations/` before running deploy
- Build with `cmake --build <dir> --target translations`

---

## CI/CD Integration

Example GitHub Actions workflow:

```yaml
name: Build and Deploy

on:
  push:
    tags:
      - 'v*'

jobs:
  build-windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - uses: jurplel/install-qt-action@v3
        with:
          version: '6.6.0'
      - run: cmake --preset windows-release
      - run: cmake --build build/windows-release
      - run: deploy\deploy_win.bat
      - uses: actions/upload-artifact@v3
        with:
          name: windows-release
          path: deploy/Release/

  build-macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
      - uses: jurplel/install-qt-action@v3
      - run: cmake --preset macos-release
      - run: cmake --build build/macos-release
      - run: chmod +x deploy/deploy_mac.sh && ./deploy/deploy_mac.sh
      - uses: actions/upload-artifact@v3
        with:
          name: macos-release
          path: deploy/Release/*.dmg

  build-linux:
    runs-on: ubuntu-20.04  # Use older Ubuntu for glibc compatibility
    steps:
      - uses: actions/checkout@v4
      - uses: jurplel/install-qt-action@v3
      - run: cmake --preset linux-release
      - run: cmake --build build/linux-release
      - run: chmod +x deploy/deploy_linux.sh && ./deploy/deploy_linux.sh
      - uses: actions/upload-artifact@v3
        with:
          name: linux-release
          path: deploy/Release/*.AppImage
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-01-07 | Initial deployment scripts |
