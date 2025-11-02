#!/bin/bash
# NeonWM Setup Script for Arch Linux
# This installs all required dependencies

set -e

echo "╔═══════════════════════════════════════════════════╗"
echo "║          NeonWM Dependency Installer              ║"
echo "╚═══════════════════════════════════════════════════╝"
echo ""

# Check if running on Arch
if ! command -v pacman &> /dev/null; then
    echo "❌ This script is for Arch Linux only!"
    exit 1
fi

echo "📦 Installing dependencies..."
echo ""

# Core build tools
sudo pacman -S --needed --noconfirm \
    base-devel \
    cmake \
    ninja \
    git \
    pkgconf \
    meson

# Wayland core
sudo pacman -S --needed --noconfirm \
    wayland \
    wayland-protocols

# wlroots dependencies
sudo pacman -S --needed --noconfirm \
    libdrm \
    libxcb \
    xcb-util-wm \
    xcb-util-renderutil \
    libx11 \
    hwdata \
    libliftoff \
    libdisplay-info

# wlroots (compositor library) - build from source
echo "📦 Building wlroots from source..."

WLROOTS_DIR="/tmp/wlroots-build"
rm -rf "$WLROOTS_DIR"  # Clean any previous failed builds
git clone https://gitlab.freedesktop.org/wlroots/wlroots.git "$WLROOTS_DIR"
cd "$WLROOTS_DIR"
git checkout 0.16.2  # Use stable 0.16 branch
meson setup build/ --prefix=/usr --buildtype=release -Dexamples=false
ninja -C build/
sudo ninja -C build/ install
sudo ldconfig
cd -
echo "✅ wlroots installed successfully"

# Graphics libraries
sudo pacman -S --needed --noconfirm \
    mesa \
    libglvnd \
    glu

# Input handling
sudo pacman -S --needed --noconfirm \
    libxkbcommon \
    libinput

# Image/rendering utilities
sudo pacman -S --needed --noconfirm \
    pixman \
    cairo \
    pango

# Optional but recommended
sudo pacman -S --needed --noconfirm \
    seatd \
    xorg-xwayland

echo ""
echo "✅ All dependencies installed!"
echo ""
echo "📋 Next steps:"
echo "   1. mkdir build && cd build"
echo "   2. cmake .."
echo "   3. make -j$(nproc)"
echo "   4. sudo make install"
echo ""
echo "🚀 Then run: neonwm"
echo ""
