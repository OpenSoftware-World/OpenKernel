#!/bin/bash

echo "======================================"
echo "  OpenKernel Dev Environment Setup"
echo "======================================"
echo ""

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root"
    exit 1
fi

if command -v apt >/dev/null 2>&1; then
    PKG="apt"
elif command -v dnf >/dev/null 2>&1; then
    PKG="dnf"
elif command -v pacman >/dev/null 2>&1; then
    PKG="pacman"
else
    echo "⚠ No supported package manager found (apt, dnf, pacman)"
    exit 1
fi

echo "✔ Detected package manager: $PKG"
echo ""

PACKAGES_APT="nasm build-essential gcc g++ make qemu qemu-system-x86 grub-pc-bin xorriso binutils git"
PACKAGES_DNF="nasm gcc gcc-c++ make qemu-system-x86 grub2-tools xorriso binutils git"
PACKAGES_PACMAN="nasm base-devel gcc make qemu grub xorriso binutils git"

AUDIO_APT="pulseaudio"
AUDIO_DNF="pulseaudio"
AUDIO_PACMAN="pulseaudio"

echo "🔄 System Update..."

case $PKG in
    apt)
        apt update && apt upgrade -y
        ;;
    dnf)
        dnf upgrade -y
        ;;
    pacman)
        pacman -Syu --noconfirm
        ;;
esac

echo "✔ System updated"
echo ""

echo "📦 Install Packages..."

case $PKG in
    apt)
        apt install -y $PACKAGES_APT
        ;;
    dnf)
        dnf install -y $PACKAGES_DNF
        ;;
    pacman)
        pacman -S --noconfirm $PACKAGES_PACMAN
        ;;
esac

echo "✔ Development tools installed"
echo ""

echo "🔊 Audio packages setup..."

case $PKG in
    apt)
        apt install -y $AUDIO_APT
        ;;
    dnf)
        dnf install -y $AUDIO_DNF
        ;;
    pacman)
        pacman -S --noconfirm $AUDIO_PACMAN
        ;;
esac

echo "✔ Audio packages installed"
echo ""

echo "🎉 OpenKernel development environment setup complete!"