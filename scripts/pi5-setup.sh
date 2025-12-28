#!/bin/bash
# Pi 5 Setup Script for Motion with libcamera support
# Target: Raspberry Pi 5 + Camera Module v3 (IMX708)
# OS: Raspberry Pi OS Bookworm

set -e

echo "========================================"
echo " Motion Pi 5 Setup Script"
echo " Target: Pi 5 + Camera Module v3"
echo "========================================"
echo ""

# Check if running on Pi
if [ ! -f /proc/device-tree/model ]; then
    echo "Warning: Not running on a Raspberry Pi"
fi

# Update system first
echo "[1/6] Updating system packages..."
sudo apt update && sudo apt upgrade -y

# Install build essentials
echo "[2/6] Installing build tools..."
sudo apt install -y \
    build-essential \
    autoconf \
    automake \
    libtool \
    pkgconf \
    gettext \
    git

# Install required dependencies
echo "[3/6] Installing required dependencies..."
sudo apt install -y \
    libjpeg-dev \
    libmicrohttpd-dev \
    zlib1g-dev

# Install FFmpeg dependencies (required)
echo "[4/6] Installing FFmpeg dependencies..."
sudo apt install -y \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libavdevice-dev

# Install libcamera (critical for Pi 5 Camera v3)
echo "[5/6] Installing libcamera..."
sudo apt install -y \
    libcamera-dev \
    libcamera-tools \
    libcamera-v4l2

# Install optional but recommended dependencies
echo "[6/6] Installing optional dependencies..."
sudo apt install -y \
    libsqlite3-dev \
    libwebp-dev \
    dos2unix

echo ""
echo "========================================"
echo " Dependency Installation Complete!"
echo "========================================"
echo ""

# Verify libcamera installation
echo "Verifying libcamera installation..."
if pkgconf --exists libcamera; then
    LIBCAM_VER=$(pkgconf --modversion libcamera)
    echo "  libcamera: OK (version $LIBCAM_VER)"
else
    echo "  libcamera: MISSING - check installation"
    exit 1
fi

# Check for camera (rpicam-* tools on Trixie, libcamera-* on Bookworm)
echo ""
echo "Checking for connected cameras..."
if command -v rpicam-hello &> /dev/null; then
    echo "  Running rpicam-hello --list-cameras..."
    rpicam-hello --list-cameras 2>&1 || true
elif command -v libcamera-hello &> /dev/null; then
    echo "  Running libcamera-hello --list-cameras..."
    libcamera-hello --list-cameras 2>&1 || true
else
    echo "  No camera tools found (rpicam-hello or libcamera-hello)"
fi

echo ""
echo "========================================"
echo " Setup complete!"
echo ""
echo " Next steps:"
echo "   1. Transfer the motion source code to ~/motion"
echo "   2. Run: cd ~/motion && ./scripts/pi5-build.sh"
echo "========================================"
