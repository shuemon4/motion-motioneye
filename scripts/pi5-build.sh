#!/bin/bash
# Pi 5 Build Script for Motion
# Run this after pi5-setup.sh and transferring source code

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "========================================"
echo " Motion Build Script for Pi 5"
echo " Project: $PROJECT_DIR"
echo "========================================"
echo ""

cd "$PROJECT_DIR"

# Fix Windows line endings if transferred from Windows
echo "[1/5] Fixing line endings (if from Windows)..."
if command -v dos2unix &> /dev/null; then
    find . -type f \( -name "*.sh" -o -name "configure*" -o -name "Makefile*" -o -name "*.ac" -o -name "*.am" -o -name "*.in" \) -exec dos2unix {} \; 2>/dev/null || true
    dos2unix scripts/* 2>/dev/null || true
fi

# Ensure scripts are executable
echo "[2/5] Setting executable permissions..."
chmod +x scripts/* 2>/dev/null || true
chmod +x configure 2>/dev/null || true

# Run autoreconf
echo "[3/5] Running autoreconf..."
autoreconf -fiv

# Configure for Pi 5 with libcamera
echo "[4/5] Configuring for Pi 5 + Camera v3..."
./configure \
    --with-libcam \
    --with-sqlite3 \
    --with-webp \
    --without-v4l2 \
    --without-mysql \
    --without-mariadb \
    --without-pgsql \
    --without-opencv \
    --without-alsa \
    --without-pulse \
    --without-fftw3

# Note: V4L2 disabled on Pi 5 because:
# 1. Pi 5 requires libcamera for CSI cameras (legacy camera stack removed)
# 2. USB cameras can still work via libcamera's UVC support
# For Pi 4 with USB cameras, use scripts/pi4-build.sh instead

# Build
echo "[5/5] Building (this may take a few minutes)..."
make -j4

echo ""
echo "========================================"
echo " Build Complete!"
echo "========================================"
echo ""
echo " Binary location: $PROJECT_DIR/src/motion"
echo ""
echo " Test run (foreground, debug mode):"
echo "   ./src/motion -n -d"
echo ""
echo " Or with a config file:"
echo "   ./src/motion -c motion.conf -n -d"
echo ""
echo " Install system-wide (optional):"
echo "   sudo make install"
echo "========================================"
