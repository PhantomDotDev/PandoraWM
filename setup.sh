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

# wlroots (compositor library)
echo "Checking for wlroots..."
if pacman -Ss wlroots | grep -q "extra/wlroots"; then
    sudo pacman -S --needed --noconfirm wlroots
elif pacman -Ss wlroots | grep -q "community/wlroots"; then
    sudo pacman -S --needed --noconfirm wlroots
else
    echo "⚠️  wlroots not found in official repos, installing from AUR..."
    if ! command -v yay &> /dev/null && ! command -v paru &> /dev/null; then
        echo "Installing yay (AUR helper)..."
        cd /tmp
        git clone https://aur.archlinux.org/yay.git
        cd yay
        makepkg -si --noconfirm
        cd -
    fi
    
    if command -v yay &> /dev/null; then
        yay -S --needed --noconfirm wlroots
    elif command -v paru &> /dev/null; then
        paru -S --needed --noconfirm wlroots
    else
        echo "❌ Could not install wlroots automatically"
        echo "Please install manually: yay -S wlroots"
        exit 1
    fi
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
