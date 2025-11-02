#!/bin/bash
# NeonWM Setup Script for Arch Linux

set -e

echo "╔═══════════════════════════════════════════════════╗"
echo "║          NeonWM Dependency Installer              ║"
echo "╚═══════════════════════════════════════════════════╝"
echo ""

# Ensure Arch Linux
if ! command -v pacman &> /dev/null; then
    echo "❌ This script is for Arch Linux only!"
    exit 1
fi

echo "📦 Installing dependencies..."
echo ""

# Base tools
sudo pacman -S --needed --noconfirm \
    base-devel cmake ninja git pkgconf \
    wayland wayland-protocols \
    mesa libglvnd glu \
    libxkbcommon libinput \
    pixman cairo pango \
    seatd xorg-xwayland

# Detect wlroots versioned package (e.g., wlroots0.18)
echo "🔍 Checking for wlroots package..."
WLR_PKG=$(pacman -Ssq "^wlroots0\.[0-9]+$" | sort -V | tail -n1)

if [[ -n "$WLR_PKG" ]]; then
    echo "✅ Found wlroots package in repo: $WLR_PKG"
    sudo pacman -S --needed --noconfirm "$WLR_PKG"
else
    echo "⚠️ wlroots not in repo — installing from AUR..."

    # Install AUR helper if missing
    if ! command -v yay &> /dev/null && ! command -v paru &> /dev/null; then
        echo "📥 Installing yay (AUR helper)..."
        cd /tmp && git clone https://aur.archlinux.org/yay.git
        cd yay && makepkg -si --noconfirm
        cd -
    fi

    # Install wlroots (AUR)
    if command -v yay &> /dev/null; then
        yay -S --needed --noconfirm wlroots
    else
        paru -S --needed --noconfirm wlroots
    fi
fi

echo ""
echo "✅ All dependencies installed!"
echo ""
echo "📋 Next steps:"
echo "   mkdir build && cd build"
echo "   cmake .."
echo "   make -j$(nproc)"
echo "   sudo make install"
echo ""
echo "🚀 Then run: neonwm"
echo ""
