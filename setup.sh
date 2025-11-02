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
    pkgconf

# Wayland core
sudo pacman -S --needed --noconfirm \
    wayland \
    wayland-protocols

# wlroots (compositor library) - build from source
echo "📦 Building wlroots from source..."
sudo pacman -S --needed --noconfirm meson

WLROOTS_DIR="/tmp/wlroots-build"
if [ ! -d "$WLROOTS_DIR" ]; then
    git clone https://gitlab.freedesktop.org/wlroots/wlroots.git "$WLROOTS_DIR"
    cd "$WLROOTS_DIR"
    git checkout 0.17.0
    meson setup build/ --prefix=/usr --buildtype=release
    ninja -C build/
    sudo ninja -C build/ install
    cd -
    echo "✅ wlroots installed successfully"
else
    echo "✅ wlroots already built (skipping)"
fi

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
