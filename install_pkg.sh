#!/bin/bash
#
# OpenKernel development environment setup.
#
# Prepares a fresh Linux machine for OpenKernel development: installs every
# tool the Makefile needs (nasm, grub-mkrescue, xorriso, QEMU, ...) and
# builds the i686-elf cross compiler (i686-elf-gcc, i686-elf-ld) when it is
# not already installed, so that `make`, `make iso` and `make run` work
# out of the box.
#
# Supported package managers: apt (Debian/Ubuntu), dnf (Fedora), pacman (Arch).
#
# Usage:
#   sudo ./install_pkg.sh [options]
#
# Options:
#   --skip-update   Do not refresh the package lists before installing
#   --upgrade       Also perform a full system upgrade before installing
#                   (pacman always runs a full upgrade: partial upgrades
#                   are unsupported on Arch Linux)
#   --skip-cross    Do not build the i686-elf cross compiler ('make' fails)
#   --verify-only   Print the tool checklist only; installs nothing
#                   (does not require root)
#   -h, --help      Show this help and exit
#
# Environment variables:
#   OPENKERNEL_GCC_VERSION       GCC version to build      (default: 14.2.0)
#   OPENKERNEL_BINUTILS_VERSION  binutils version to build (default: 2.43)
#   OPENKERNEL_CROSS_BUILD_DIR   Build directory
#                                (default: /var/tmp/openkernel-cross)

# Fail early with a clear message when run through a POSIX shell (sh/dash)
if [ -z "${BASH_VERSION:-}" ]; then
    echo "This script must be run with bash. Try: bash install_pkg.sh" >&2
    exit 1
fi

set -Eeuo pipefail

# ---------------------------------------------------------------------------
# Options and configuration
# ---------------------------------------------------------------------------
SKIP_UPDATE=0
UPGRADE=0
SKIP_CROSS=0
VERIFY_ONLY=0

GCC_VERSION="${OPENKERNEL_GCC_VERSION:-14.2.0}"
BINUTILS_VERSION="${OPENKERNEL_BINUTILS_VERSION:-2.43}"
CROSS_PREFIX="/usr/local"
WORK_DIR="${OPENKERNEL_CROSS_BUILD_DIR:-/var/tmp/openkernel-cross}"

# Make sure locally installed tools (i686-elf-*, grub-mkrescue alias) are found
export PATH="$CROSS_PREFIX/bin:${PATH:-/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin}"

# Tools the Makefile invokes directly; everything here must exist at the end
REQUIRED_TOOLS=(nasm make i686-elf-gcc i686-elf-ld qemu-system-i386 qemu-img grub-mkrescue xorriso)
# Nice to have, not needed by the build itself
OPTIONAL_TOOLS=(git)

# ---------------------------------------------------------------------------
# Package lists (one set per package manager)
# ---------------------------------------------------------------------------
PACKAGES_APT=(
    # Host toolchain: gcc, g++, make, binutils (builds the cross compiler)
    build-essential
    # Tools used by the Makefile
    nasm
    xorriso
    grub-pc-bin        # grub-mkrescue + the 32-bit GRUB modules
    mtools             # used by grub-mkrescue for FAT/EFI images
    qemu-system-x86    # qemu-system-i386
    qemu-utils         # qemu-img (creates disk.img)
    # Cross-compiler build dependencies
    texinfo
    libgmp-dev
    libmpfr-dev
    libmpc-dev
    # Download helpers
    curl
    xz-utils
    ca-certificates   # HTTPS certificates for the cross-compiler download
    # Version control
    git
)
AUDIO_APT=(libpulse0) # provides libpulse0 for QEMU's '-audiodev pa'

PACKAGES_DNF=(
    # Host toolchain: gcc, g++, make (binutils comes in as a gcc dependency)
    gcc
    gcc-c++
    make
    # Tools used by the Makefile
    nasm
    xorriso
    grub2-tools-extra   # grub2-mkrescue
    grub2-pc-modules    # /usr/lib/grub/i386-pc
    mtools
    qemu-system-x86
    qemu-img
    # Cross-compiler build dependencies
    texinfo
    gmp-devel
    mpfr-devel
    libmpc-devel
    # Download helpers
    curl
    xz
    # Version control
    git
)
AUDIO_DNF=(pulseaudio-libs)

PACKAGES_PACMAN=(
    # Host toolchain: gcc, g++, make, binutils, texinfo
    base-devel
    # Tools used by the Makefile
    nasm
    grub                # grub-mkrescue + /usr/lib/grub/i386-pc
    xorriso
    mtools
    qemu-system-x86
    qemu-img
    # Cross-compiler build dependencies (headers ship with these packages)
    gmp
    mpfr
    libmpc
    # Download helpers
    curl
    xz
    # Version control
    git
)
AUDIO_PACMAN=(libpulse)

# ---------------------------------------------------------------------------
# Output helpers
# ---------------------------------------------------------------------------
if [ -t 1 ]; then
    C_RESET=$'\033[0m'
    C_RED=$'\033[31m'
    C_GREEN=$'\033[32m'
    C_YELLOW=$'\033[33m'
    C_BLUE=$'\033[34m'
else
    C_RESET=""
    C_RED=""
    C_GREEN=""
    C_YELLOW=""
    C_BLUE=""
fi

info()    { printf '%sℹ %s%s\n' "$C_BLUE" "$*" "$C_RESET"; }
success() { printf '%s✔ %s%s\n' "$C_GREEN" "$*" "$C_RESET"; }
warn()    { printf '%s⚠ %s%s\n' "$C_YELLOW" "$*" "$C_RESET"; }
error()   { printf '%s✖ %s%s\n' "$C_RED" "$*" "$C_RESET" >&2; }
die()     { error "$*"; exit 1; }

# Reports the failing line and keeps cross-compiler build files for inspection
on_error() {
    error "Unexpected failure on line $1."
    if [ -d "$WORK_DIR/build-gcc" ] || [ -d "$WORK_DIR/build-binutils" ]; then
        warn "Cross-compiler build files were kept in $WORK_DIR (remove the directory manually when done)."
    fi
}
trap 'on_error "$LINENO"' ERR

usage() {
    cat <<'EOF'
OpenKernel development environment setup

Usage:
  sudo ./install_pkg.sh [options]

Options:
  --skip-update   Do not refresh the package lists before installing
  --upgrade       Also perform a full system upgrade before installing
                  (pacman always runs a full upgrade; Arch does not
                  support partial upgrades)
  --skip-cross    Do not build the i686-elf cross compiler ('make' fails)
  --verify-only   Print the tool checklist only; installs nothing
                  (does not require root)
  -h, --help      Show this help and exit

Environment variables:
  OPENKERNEL_GCC_VERSION        GCC version to build      (default: 14.2.0)
  OPENKERNEL_BINUTILS_VERSION   binutils version to build (default: 2.43)
  OPENKERNEL_CROSS_BUILD_DIR    Build directory
                                (default: /var/tmp/openkernel-cross)
EOF
}

# ---------------------------------------------------------------------------
# Steps
# ---------------------------------------------------------------------------

# Detects the package manager of the host system
detect_pkg_manager() {
    if command -v apt-get >/dev/null 2>&1; then
        PKG="apt"
    elif command -v dnf >/dev/null 2>&1; then
        PKG="dnf"
    elif command -v pacman >/dev/null 2>&1; then
        PKG="pacman"
    else
        die "No supported package manager found (apt, dnf, pacman)."
    fi
    success "Detected package manager: $PKG"
}

# Refreshes the package metadata (and optionally upgrades the system)
update_package_index() {
    if [ "$SKIP_UPDATE" = 1 ]; then
        if [ "$UPGRADE" = 1 ]; then
            die "--skip-update and --upgrade cannot be combined."
        fi
        info "Skipping package list refresh (--skip-update)."
        return 0
    fi

    if [ "$PKG" = "pacman" ]; then
        # Refresh and upgrade in one step: installing right after a bare -Sy
        # would leave Arch in an unsupported "partial upgrade" state.
        info "Refreshing package lists and upgrading the system (pacman)..."
        pacman -Syu --noconfirm
        success "Package lists refreshed and system upgraded"
        return 0
    fi

    info "Refreshing package lists..."
    case "$PKG" in
        apt)
            apt-get update
            ;;
        dnf)
            dnf makecache
            ;;
    esac
    success "Package lists refreshed"

    if [ "$UPGRADE" = 1 ]; then
        info "Upgrading the system..."
        case "$PKG" in
            apt)
                apt-get upgrade -y
                ;;
            dnf)
                dnf upgrade -y
                ;;
        esac
        success "System upgraded"
    fi
}

# Installs the packages required by the Makefile and the cross build
install_packages() {
    info "Installing packages..."
    case "$PKG" in
        apt)
            DEBIAN_FRONTEND=noninteractive apt-get install -y "${PACKAGES_APT[@]}"
            ;;
        dnf)
            dnf install -y "${PACKAGES_DNF[@]}"
            ;;
        pacman)
            pacman -S --noconfirm --needed "${PACKAGES_PACMAN[@]}"
            ;;
    esac
    success "Packages installed"
}

# Installs the PulseAudio client library QEMU's '-audiodev pa' needs.
# Best effort: a conflict with PipeWire must not abort the whole setup.
install_audio_support() {
    info "Installing PulseAudio client library (QEMU '-audiodev pa')..."
    case "$PKG" in
        apt)
            DEBIAN_FRONTEND=noninteractive apt-get install -y "${AUDIO_APT[@]}" ||
                warn "PulseAudio package could not be installed; 'make run' audio may not work."
            ;;
        dnf)
            dnf install -y "${AUDIO_DNF[@]}" ||
                warn "PulseAudio package could not be installed; 'make run' audio may not work."
            ;;
        pacman)
            pacman -S --noconfirm --needed "${AUDIO_PACMAN[@]}" ||
                warn "PulseAudio package could not be installed; 'make run' audio may not work."
            ;;
    esac
}

# Fedora ships the tool as grub2-mkrescue, but the Makefile calls grub-mkrescue
ensure_grub_mkrescue() {
    if command -v grub-mkrescue >/dev/null 2>&1; then
        return 0
    fi
    if command -v grub2-mkrescue >/dev/null 2>&1; then
        mkdir -p "$CROSS_PREFIX/bin"
        ln -sf "$(command -v grub2-mkrescue)" "$CROSS_PREFIX/bin/grub-mkrescue"
        success "grub-mkrescue alias created -> grub2-mkrescue"
        return 0
    fi
    warn "grub-mkrescue was not found; 'make iso' will fail."
}

# Downloads a file with curl; finished files are reused on later runs
download_file() {
    local url="$1"
    local dest="$2"

    if [ -s "$dest" ]; then
        info "Using cached $(basename "$dest")"
        return 0
    fi
    info "Downloading $url"
    curl -fL --retry 3 --connect-timeout 30 -o "$dest.part" "$url"
    mv "$dest.part" "$dest"
}

# Extracts a source tarball exactly once. A marker file makes interrupted
# extractions safe: a partial directory is dropped and rebuilt on the next
# run instead of being mistaken for a complete extraction.
extract_once() {
    local tarball="$1"
    local extract_dir="$2"
    local name="$3"
    local marker="$extract_dir/.${name}-extracted"

    if [ -f "$marker" ] && [ -d "$extract_dir/$name" ]; then
        return 0
    fi
    info "Extracting $name..."
    rm -rf "${extract_dir:?}/$name"
    if ! tar -xf "$tarball" -C "$extract_dir"; then
        # Drop the archive so the next run downloads a fresh copy
        rm -f "$tarball"
        die "Failed to extract $(basename "$tarball") — the download may be corrupt. It has been deleted; run the script again to re-download it."
    fi
    touch "$marker"
}

# Returns 0 when i686-elf-gcc and i686-elf-ld exist and can actually
# compile and link a freestanding test program
cross_compiler_works() {
    command -v i686-elf-gcc >/dev/null 2>&1 || return 1
    command -v i686-elf-ld >/dev/null 2>&1 || return 1

    local tmp
    tmp="$(mktemp -d)" || return 1
    if printf 'void kernel_entry(void) {}\n' > "$tmp/smoke.c" &&
        i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -c "$tmp/smoke.c" -o "$tmp/smoke.o" 2>/dev/null &&
        i686-elf-ld -m elf_i386 -e kernel_entry "$tmp/smoke.o" -o "$tmp/smoke.elf" 2>/dev/null; then
        rm -rf "$tmp"
        return 0
    fi
    rm -rf "$tmp"
    return 1
}

# Builds and installs the i686-elf cross compiler (binutils + GCC) from GNU
# sources when i686-elf-gcc/i686-elf-ld are not already available.
ensure_cross_compiler() {
    if cross_compiler_works; then
        success "i686-elf cross compiler already installed ($(i686-elf-gcc --version | head -n1))"
        return 0
    fi

    if command -v i686-elf-gcc >/dev/null 2>&1; then
        warn "i686-elf-gcc is installed but failed the smoke test; rebuilding it."
    fi

    if [ "$SKIP_CROSS" = 1 ]; then
        warn "--skip-cross given: i686-elf-gcc/i686-elf-ld were NOT installed, 'make' will fail."
        return 0
    fi

    info "Cross compiler not found: building binutils $BINUTILS_VERSION + GCC $GCC_VERSION for i686-elf."
    warn "The first run takes roughly 15-40 minutes; later runs skip this step."

    local t
    for t in gcc g++ make curl tar xz; do
        command -v "$t" >/dev/null 2>&1 ||
            die "Host tool '$t' is missing; it should come with the packages — install it manually and run the script again."
    done

    local jobs
    jobs="${OPENKERNEL_BUILD_JOBS:-$(nproc 2>/dev/null || echo 2)}"

    mkdir -p "$WORK_DIR"

    # GCC sources, build trees and logs need several GB of disk space
    local avail_kb
    avail_kb="$(df -k --output=avail "$WORK_DIR" | tail -n1 | tr -d ' ' || true)"
    case "$avail_kb" in
        ''|*[!0-9]*)
            warn "Could not determine free disk space in $WORK_DIR; continuing anyway."
            ;;
        *)
            if [ "$avail_kb" -lt 4194304 ]; then
                die "Not enough free disk space in $WORK_DIR (need ~4 GB, found $((avail_kb / 1024)) MB). Set OPENKERNEL_CROSS_BUILD_DIR to a larger filesystem."
            fi
            ;;
    esac

    local src="$WORK_DIR/src"
    local extract="$WORK_DIR/extract"
    mkdir -p "$src" "$extract" "$WORK_DIR/build-binutils" "$WORK_DIR/build-gcc"

    download_file "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz" \
        "$src/binutils-$BINUTILS_VERSION.tar.xz"
    download_file "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz" \
        "$src/gcc-$GCC_VERSION.tar.xz"

    extract_once "$src/binutils-$BINUTILS_VERSION.tar.xz" "$extract" "binutils-$BINUTILS_VERSION"
    extract_once "$src/gcc-$GCC_VERSION.tar.xz" "$extract" "gcc-$GCC_VERSION"

    info "Building binutils (log: $WORK_DIR/build-binutils.log)..."
    if ! (
        cd "$WORK_DIR/build-binutils" &&
        "$extract/binutils-$BINUTILS_VERSION/configure" \
            --target=i686-elf \
            --prefix="$CROSS_PREFIX" \
            --disable-nls \
            --disable-werror \
            --disable-gprofng &&
        make -j"$jobs" &&
        make install
    ) 2>&1 | tee "$WORK_DIR/build-binutils.log"; then
        tail -n 20 "$WORK_DIR/build-binutils.log" >&2 || true
        die "binutils build failed (full log: $WORK_DIR/build-binutils.log)."
    fi

    info "Building GCC (log: $WORK_DIR/build-gcc.log, this is the slow part)..."
    if ! (
        cd "$WORK_DIR/build-gcc" &&
        "$extract/gcc-$GCC_VERSION/configure" \
            --target=i686-elf \
            --prefix="$CROSS_PREFIX" \
            --enable-languages=c \
            --without-headers \
            --disable-multilib \
            --disable-nls \
            --disable-werror \
            --disable-threads \
            --disable-libssp \
            --disable-libgomp \
            --disable-libquadmath &&
        make -j"$jobs" all-gcc all-target-libgcc &&
        make install-gcc install-target-libgcc
    ) 2>&1 | tee "$WORK_DIR/build-gcc.log"; then
        tail -n 20 "$WORK_DIR/build-gcc.log" >&2 || true
        die "GCC build failed (full log: $WORK_DIR/build-gcc.log)."
    fi

    info "Smoke-testing the new toolchain..."
    if ! cross_compiler_works; then
        die "The cross compiler was built but failed to compile a test file (see logs above)."
    fi

    success "i686-elf cross compiler installed ($(i686-elf-gcc --version | head -n1))"

    # Keep the downloaded tarballs and logs, drop the multi-GB build trees
    rm -rf "$WORK_DIR/build-binutils" "$WORK_DIR/build-gcc" "$extract"
}

# Prints a checklist of every tool the Makefile needs; returns non-zero if a
# required tool is missing.
verify_tools() {
    local missing=0
    local t path

    echo ""
    info "Verification (tools used by the Makefile):"
    for t in "${REQUIRED_TOOLS[@]}"; do
        if path="$(command -v "$t" 2>/dev/null)"; then
            printf '  %s✔ %-18s%s %s\n' "$C_GREEN" "$t" "$C_RESET" "$path"
        else
            printf '  %s✖ %-18s missing%s\n' "$C_RED" "$t" "$C_RESET"
            missing=$((missing + 1))
        fi
    done

    # A cross compiler that cannot compile is as good as missing
    if command -v i686-elf-gcc >/dev/null 2>&1; then
        if cross_compiler_works; then
            printf '  %s✔ %-18s%s compiles and links a test file\n' \
                "$C_GREEN" "i686-elf-gcc test" "$C_RESET"
        else
            printf '  %s✖ %-18s installed but failed to compile%s\n' \
                "$C_RED" "i686-elf-gcc test" "$C_RESET"
            missing=$((missing + 1))
        fi
    fi

    # grub-mkrescue needs the 32-bit PC modules to build the ISO
    if [ -d /usr/lib/grub/i386-pc ]; then
        printf '  %s✔ %-18s%s /usr/lib/grub/i386-pc\n' "$C_GREEN" "grub i386-pc modules" "$C_RESET"
    else
        printf '  %s✖ %-18s missing (grub-pc-bin / grub2-pc-modules / grub)%s\n' \
            "$C_RED" "grub i386-pc modules" "$C_RESET"
        missing=$((missing + 1))
    fi

    for t in "${OPTIONAL_TOOLS[@]}"; do
        if path="$(command -v "$t" 2>/dev/null)"; then
            printf '  %s✔ %-18s%s %s (optional)\n' "$C_GREEN" "$t" "$C_RESET" "$path"
        else
            printf '  %s⚠ %-18s missing (optional)%s\n' "$C_YELLOW" "$t" "$C_RESET"
        fi
    done

    # 'make run' attaches a Sound Blaster 16 through QEMU's '-audiodev pa'
    local qemu_bin=""
    qemu_bin="$(command -v qemu-system-i386 2>/dev/null || true)"
    if [ -n "$qemu_bin" ] && command -v ldd >/dev/null 2>&1; then
        if ldd "$qemu_bin" 2>/dev/null | grep libpulse >/dev/null; then
            printf '  %s✔ %-18s%s linked against libpulse\n' "$C_GREEN" "qemu pa backend" "$C_RESET"
        else
            printf '  %s⚠ %-18s QEMU has no PulseAudio backend, "make run" may fail%s\n' \
                "$C_YELLOW" "qemu pa backend" "$C_RESET"
        fi
    fi

    if [ "$missing" -gt 0 ]; then
        echo ""
        warn "$missing required tool(s) missing. Run 'sudo ./install_pkg.sh' to install them."
        return 1
    fi
    return 0
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --skip-update) SKIP_UPDATE=1 ;;
        --upgrade) UPGRADE=1 ;;
        --skip-cross) SKIP_CROSS=1 ;;
        --verify-only) VERIFY_ONLY=1 ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "Unknown option: $1 (use --help)"
            ;;
    esac
    shift
done

printf '%s\n' "======================================"
printf '%s\n' "  OpenKernel Dev Environment Setup"
printf '%s\n' "======================================"
echo ""

if [ "$VERIFY_ONLY" = 1 ]; then
    verify_tools || exit 1
    echo ""
    success "All required tools are present."
    exit 0
fi

if [ "$EUID" -ne 0 ]; then
    die "Please run as root: sudo ./install_pkg.sh"
fi

detect_pkg_manager
update_package_index
install_packages
install_audio_support
ensure_grub_mkrescue
ensure_cross_compiler

if verify_tools; then
    echo ""
    success "OpenKernel development environment is ready!"
    info "Next steps: make && make iso && make run"
else
    exit 1
fi
