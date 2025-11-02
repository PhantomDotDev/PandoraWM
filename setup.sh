#!/bin/bash
# NeonWM Simple Setup - NO WLROOTS NEEDED
# This version is much easier to build

set -e

echo "╔═══════════════════════════════════════════════════╗"
echo "║     NeonWM Simple Version - Easy Install          ║"
echo "╚═══════════════════════════════════════════════════╝"
echo ""

# Check if running on Arch
if ! command -v pacman &> /dev/null; then
    echo "❌ This script is for Arch Linux only!"
    exit 1
fi

echo "📦 Installing minimal dependencies..."
echo ""

# Only essential dependencies - NO wlroots!
sudo pacman -S --needed --noconfirm \
    base-devel \
    cmake \
    git \
    pkgconf \
    wayland \
    wayland-protocols \
    libxkbcommon \
    libinput \
    mesa \
    glfw-wayland \
    glew \
    glm \
    libudev.so

echo ""
echo "✅ All dependencies installed!"
echo ""
echo "📋 Next steps:"
echo "   1. cd neonwm"
echo "   2. mkdir build && cd build"
echo "   3. cmake .."
echo "   4. make -j$(nproc)"
echo ""
echo "🚀 This version is much simpler and builds in 30 seconds!"
echo ""
