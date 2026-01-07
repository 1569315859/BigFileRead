#!/bin/bash
# ============================================================================
# BigFileViewer macOS Deployment Script
# ============================================================================
# This script uses macdeployqt to bundle all Qt frameworks into the .app
# bundle and optionally creates a .dmg installer image.
#
# Prerequisites:
#   - Qt installed (Homebrew or official installer)
#   - BigFileViewer.app already built
#   - Xcode Command Line Tools installed
#
# Usage:
#   chmod +x deploy_mac.sh
#   ./deploy_mac.sh
#
# Output:
#   - BigFileViewer.app (self-contained application bundle)
#   - BigFileViewer.dmg (optional disk image installer)
# ============================================================================

set -e  # Exit on error

# ============================================================================
# Configuration - Modify these paths according to your environment
# ============================================================================

# Qt installation path (adjust to your Qt version)
# For Homebrew Qt: /opt/homebrew/opt/qt (Apple Silicon) or /usr/local/opt/qt (Intel)
# For official Qt: ~/Qt/6.x.x/macos
QT_DIR="${QT_DIR:-/opt/homebrew/opt/qt}"

# Build output directory
BUILD_DIR="$(dirname "$0")/../bin"

# Deployment output directory
DEPLOY_DIR="$(dirname "$0")/Release"

# Application name
APP_NAME="BigFileViewer"

# Whether to create a DMG (set to "yes" or "no")
CREATE_DMG="yes"

# Whether to sign the app (requires Apple Developer ID)
SIGN_APP="no"
# DEVELOPER_ID="Developer ID Application: Your Name (XXXXXXXXXX)"

# ============================================================================
# Color output helpers
# ============================================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# ============================================================================
# Detect Qt installation
# ============================================================================
detect_qt() {
    # Try common locations
    local qt_paths=(
        "$QT_DIR"
        "/opt/homebrew/opt/qt"
        "/usr/local/opt/qt"
        "$HOME/Qt/6.7.0/macos"
        "$HOME/Qt/6.6.0/macos"
        "$HOME/Qt/6.5.0/macos"
    )
    
    for path in "${qt_paths[@]}"; do
        if [[ -f "$path/bin/macdeployqt" ]]; then
            QT_DIR="$path"
            return 0
        fi
    done
    
    # Try to find via qmake
    if command -v qmake &> /dev/null; then
        QT_DIR=$(qmake -query QT_INSTALL_PREFIX)
        if [[ -f "$QT_DIR/bin/macdeployqt" ]]; then
            return 0
        fi
    fi
    
    return 1
}

# ============================================================================
# Main Script
# ============================================================================

echo "============================================================================"
echo "BigFileViewer macOS Deployment"
echo "============================================================================"
echo

# Detect Qt
if ! detect_qt; then
    error "Could not find Qt installation. Please set QT_DIR environment variable."
fi

info "Qt Directory:    $QT_DIR"
info "Build Directory: $BUILD_DIR"
info "Deploy To:       $DEPLOY_DIR"
echo

# ============================================================================
# Step 1: Verify build output exists
# ============================================================================
info "[1/6] Verifying build output..."

if [[ ! -d "$BUILD_DIR/$APP_NAME.app" ]]; then
    error "$APP_NAME.app not found at: $BUILD_DIR/$APP_NAME.app
Please build the project first:
  cmake --preset macos-release
  cmake --build build/macos-release"
fi

# ============================================================================
# Step 2: Create/Clean deployment directory
# ============================================================================
info "[2/6] Preparing deployment directory..."

rm -rf "$DEPLOY_DIR"
mkdir -p "$DEPLOY_DIR"

# ============================================================================
# Step 3: Copy application bundle
# ============================================================================
info "[3/6] Copying application bundle..."

cp -R "$BUILD_DIR/$APP_NAME.app" "$DEPLOY_DIR/"

# Copy translation files into the bundle
if [[ -d "$BUILD_DIR/translations" ]]; then
    mkdir -p "$DEPLOY_DIR/$APP_NAME.app/Contents/Resources/translations"
    cp "$BUILD_DIR/translations/"*.qm "$DEPLOY_DIR/$APP_NAME.app/Contents/Resources/translations/" 2>/dev/null || true
    info "      Copied translation files"
fi

# ============================================================================
# Step 4: Run macdeployqt to bundle Qt frameworks
# ============================================================================
info "[4/6] Running macdeployqt to bundle Qt frameworks..."

"$QT_DIR/bin/macdeployqt" "$DEPLOY_DIR/$APP_NAME.app" \
    -verbose=1 \
    -always-overwrite

info "      Qt frameworks bundled successfully"

# ============================================================================
# Step 5: Code signing (optional)
# ============================================================================
if [[ "$SIGN_APP" == "yes" && -n "$DEVELOPER_ID" ]]; then
    info "[5/6] Signing application..."
    
    # Sign all frameworks and the main executable
    codesign --deep --force --verify --verbose \
        --sign "$DEVELOPER_ID" \
        "$DEPLOY_DIR/$APP_NAME.app"
    
    # Verify signature
    codesign --verify --verbose "$DEPLOY_DIR/$APP_NAME.app"
    info "      Application signed successfully"
else
    info "[5/6] Skipping code signing (not configured)"
fi

# ============================================================================
# Step 6: Create DMG (optional)
# ============================================================================
if [[ "$CREATE_DMG" == "yes" ]]; then
    info "[6/6] Creating DMG installer..."
    
    DMG_NAME="$APP_NAME-$(date +%Y%m%d).dmg"
    
    # Create a temporary directory for DMG contents
    DMG_TEMP="$DEPLOY_DIR/dmg_temp"
    mkdir -p "$DMG_TEMP"
    cp -R "$DEPLOY_DIR/$APP_NAME.app" "$DMG_TEMP/"
    
    # Create symbolic link to Applications folder
    ln -s /Applications "$DMG_TEMP/Applications"
    
    # Create the DMG
    hdiutil create -volname "$APP_NAME" \
        -srcfolder "$DMG_TEMP" \
        -ov -format UDZO \
        "$DEPLOY_DIR/$DMG_NAME"
    
    # Cleanup
    rm -rf "$DMG_TEMP"
    
    info "      Created: $DEPLOY_DIR/$DMG_NAME"
else
    info "[6/6] Skipping DMG creation"
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
ls -la "$DEPLOY_DIR"
echo
echo "============================================================================"
echo "Distribution Notes:"
echo "============================================================================"
echo "1. The .app bundle is self-contained and can be distributed as-is"
echo "2. For App Store distribution, you need to:"
echo "   - Sign with 'Apple Distribution' certificate"
echo "   - Enable Hardened Runtime"
echo "   - Notarize with Apple"
echo "3. For direct distribution, consider notarizing to avoid Gatekeeper warnings"
echo "============================================================================"
