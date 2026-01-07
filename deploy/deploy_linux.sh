#!/bin/bash
# ============================================================================
# BigFileViewer Linux Deployment Script
# ============================================================================
# This script uses linuxdeployqt to create a portable AppImage.
#
# Prerequisites:
#   - Qt installed
#   - BigFileViewer built
#   - linuxdeployqt downloaded (see below)
#
# Download linuxdeployqt:
#   wget https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage
#   chmod +x linuxdeployqt-continuous-x86_64.AppImage
#
# Usage:
#   chmod +x deploy_linux.sh
#   ./deploy_linux.sh
#
# Output:
#   - BigFileViewer-x86_64.AppImage (portable single-file application)
# ============================================================================

set -e  # Exit on error

# ============================================================================
# Configuration
# ============================================================================

# Application name
APP_NAME="BigFileViewer"
APP_ID="com.bigfileread.bigfileviewer"

# Build output directory
BUILD_DIR="$(dirname "$0")/../bin"

# Deployment directory
DEPLOY_DIR="$(dirname "$0")/Release"

# AppDir structure
APPDIR="$DEPLOY_DIR/AppDir"

# linuxdeployqt path (download from GitHub if not present)
LINUXDEPLOYQT="./linuxdeployqt-continuous-x86_64.AppImage"

# ============================================================================
# Color output helpers
# ============================================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# ============================================================================
# Create .desktop file
# ============================================================================
create_desktop_file() {
    cat > "$1" << EOF
[Desktop Entry]
Type=Application
Name=BigFileViewer
GenericName=Large File Viewer
Comment=High-performance viewer for large text files
Exec=BigFileViewer %F
Icon=bigfileviewer
Categories=Utility;TextEditor;Development;
Terminal=false
MimeType=text/plain;text/x-log;application/x-yaml;application/json;text/csv;
Keywords=log;viewer;text;large;file;
StartupWMClass=BigFileViewer
EOF
}

# ============================================================================
# Create AppStream metadata (optional, for software centers)
# ============================================================================
create_appdata_file() {
    mkdir -p "$(dirname "$1")"
    cat > "$1" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>${APP_ID}</id>
  <name>BigFileViewer</name>
  <summary>High-performance large file viewer</summary>
  <metadata_license>MIT</metadata_license>
  <project_license>MIT</project_license>
  <description>
    <p>
      BigFileViewer is a high-performance text file viewer designed to handle
      files of any size, including 10GB+ log files. It uses memory-mapped file
      access and asynchronous indexing to provide instant loading and smooth
      scrolling.
    </p>
    <p>Features:</p>
    <ul>
      <li>Memory-mapped file access for instant loading</li>
      <li>Asynchronous indexing with progress indication</li>
      <li>Supports files larger than 10GB</li>
      <li>VS Code-inspired dark theme</li>
      <li>Real-time log file monitoring (tail -f mode)</li>
      <li>Powerful search with regex support</li>
      <li>Line filtering and bookmarks</li>
    </ul>
  </description>
  <launchable type="desktop-id">bigfileviewer.desktop</launchable>
  <url type="homepage">https://github.com/yourusername/BigFileViewer</url>
  <screenshots>
    <screenshot type="default">
      <caption>Main window with VS Code dark theme</caption>
      <image>https://example.com/screenshot.png</image>
    </screenshot>
  </screenshots>
  <releases>
    <release version="1.5.0" date="$(date +%Y-%m-%d)">
      <description>
        <p>Initial release with core features.</p>
      </description>
    </release>
  </releases>
  <content_rating type="oars-1.1"/>
</component>
EOF
}

# ============================================================================
# Main Script
# ============================================================================

echo "============================================================================"
echo "BigFileViewer Linux Deployment (AppImage)"
echo "============================================================================"
echo

# ============================================================================
# Step 1: Check prerequisites
# ============================================================================
info "[1/7] Checking prerequisites..."

# Check for linuxdeployqt
if [[ ! -f "$LINUXDEPLOYQT" ]]; then
    warn "linuxdeployqt not found. Attempting to download..."
    
    wget -q --show-progress \
        "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage" \
        -O "$LINUXDEPLOYQT"
    chmod +x "$LINUXDEPLOYQT"
    
    if [[ ! -f "$LINUXDEPLOYQT" ]]; then
        error "Failed to download linuxdeployqt. Please download manually from:
https://github.com/probonopd/linuxdeployqt/releases"
    fi
    info "      Downloaded linuxdeployqt successfully"
fi

# Check for appimagetool (usually bundled with linuxdeployqt)
# It will be downloaded automatically if needed

# ============================================================================
# Step 2: Verify build output
# ============================================================================
info "[2/7] Verifying build output..."

if [[ ! -f "$BUILD_DIR/$APP_NAME" ]]; then
    error "$APP_NAME not found at: $BUILD_DIR/$APP_NAME
Please build the project first:
  cmake --preset linux-release
  cmake --build build/linux-release"
fi

# ============================================================================
# Step 3: Create AppDir structure
# ============================================================================
info "[3/7] Creating AppDir structure..."

rm -rf "$DEPLOY_DIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$APPDIR/usr/share/metainfo"

# ============================================================================
# Step 4: Copy executable and resources
# ============================================================================
info "[4/7] Copying executable and resources..."

# Copy main executable
cp "$BUILD_DIR/$APP_NAME" "$APPDIR/usr/bin/"
chmod +x "$APPDIR/usr/bin/$APP_NAME"
info "      Copied: $APP_NAME"

# Copy translations
if [[ -d "$BUILD_DIR/translations" ]]; then
    mkdir -p "$APPDIR/usr/share/$APP_NAME/translations"
    cp "$BUILD_DIR/translations/"*.qm "$APPDIR/usr/share/$APP_NAME/translations/" 2>/dev/null || true
    info "      Copied translation files"
fi

# Copy or create icon
# First try to find an existing icon
ICON_SOURCES=(
    "$(dirname "$0")/../src/app_icon.png"
    "$(dirname "$0")/../resources/icon.png"
    "$(dirname "$0")/../icon.png"
)

ICON_FOUND=false
for icon in "${ICON_SOURCES[@]}"; do
    if [[ -f "$icon" ]]; then
        cp "$icon" "$APPDIR/usr/share/icons/hicolor/256x256/apps/bigfileviewer.png"
        ICON_FOUND=true
        info "      Copied icon from: $icon"
        break
    fi
done

if [[ "$ICON_FOUND" == "false" ]]; then
    warn "No icon found. Creating a placeholder icon..."
    # Create a simple placeholder icon using ImageMagick if available
    if command -v convert &> /dev/null; then
        convert -size 256x256 xc:'#1e1e1e' \
            -fill '#007acc' -draw "roundrectangle 20,20,236,236,20,20" \
            -fill white -font DejaVu-Sans-Bold -pointsize 120 \
            -gravity center -annotate 0 "BF" \
            "$APPDIR/usr/share/icons/hicolor/256x256/apps/bigfileviewer.png"
    else
        warn "ImageMagick not found. You may need to add an icon manually."
    fi
fi

# Create symlink for AppImage icon
ln -sf "usr/share/icons/hicolor/256x256/apps/bigfileviewer.png" "$APPDIR/bigfileviewer.png"

# ============================================================================
# Step 5: Create .desktop file
# ============================================================================
info "[5/7] Creating .desktop file..."

create_desktop_file "$APPDIR/usr/share/applications/bigfileviewer.desktop"

# Create symlink for AppImage
ln -sf "usr/share/applications/bigfileviewer.desktop" "$APPDIR/bigfileviewer.desktop"

# Create AppStream metadata
create_appdata_file "$APPDIR/usr/share/metainfo/${APP_ID}.appdata.xml"

info "      Created desktop entry and metadata"

# ============================================================================
# Step 6: Run linuxdeployqt
# ============================================================================
info "[6/7] Running linuxdeployqt to bundle Qt libraries..."

# Set Qt plugin path if needed
export QT_PLUGIN_PATH="${QT_PLUGIN_PATH:-$(qmake -query QT_INSTALL_PLUGINS 2>/dev/null || echo '')}"

# Unset QTDIR to let linuxdeployqt find Qt automatically
unset QTDIR

# Run linuxdeployqt
# Note: -appimage flag creates the AppImage automatically
"$LINUXDEPLOYQT" "$APPDIR/usr/share/applications/bigfileviewer.desktop" \
    -bundle-non-qt-libs \
    -extra-plugins=iconengines,platformthemes/libqgtk3.so \
    -appimage

if [[ $? -ne 0 ]]; then
    error "linuxdeployqt failed!"
fi

info "      AppImage created successfully"

# ============================================================================
# Step 7: Rename and move AppImage
# ============================================================================
info "[7/7] Finalizing..."

# Find the generated AppImage
APPIMAGE=$(ls -1 BigFileViewer*.AppImage 2>/dev/null | head -1)

if [[ -f "$APPIMAGE" ]]; then
    FINAL_NAME="BigFileViewer-$(uname -m).AppImage"
    mv "$APPIMAGE" "$DEPLOY_DIR/$FINAL_NAME"
    chmod +x "$DEPLOY_DIR/$FINAL_NAME"
    info "      Created: $DEPLOY_DIR/$FINAL_NAME"
fi

# ============================================================================
# Done!
# ============================================================================
echo
echo "============================================================================"
echo "Deployment Complete!"
echo "============================================================================"
echo
echo "Output directory: $DEPLOY_DIR"
echo
echo "Contents:"
ls -la "$DEPLOY_DIR"/*.AppImage 2>/dev/null || ls -la "$DEPLOY_DIR"
echo
echo "============================================================================"
echo "Testing the AppImage:"
echo "============================================================================"
echo "  chmod +x $DEPLOY_DIR/$FINAL_NAME"
echo "  ./$DEPLOY_DIR/$FINAL_NAME"
echo
echo "Distribution Notes:"
echo "  - The AppImage is a self-contained, portable executable"
echo "  - It can run on most Linux distributions without installation"
echo "  - Users just need to: chmod +x and run"
echo "  - For Flatpak/Snap distribution, additional packaging is needed"
echo "============================================================================"
