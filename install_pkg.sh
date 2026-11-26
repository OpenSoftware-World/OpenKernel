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
# Parallel jobs for the cross-compiler build (build_component uses it)
JOBS="${OPENKERNEL_BUILD_JOBS:-$(nproc 2>/dev/null || echo 2)}"

# SHA-256 of the official GNU release tarballs, checked before every build.
# Pinned only for the default versions: overriding OPENKERNEL_*_VERSION
# turns the check off because the pinned hash would not match anymore.
GCC_SHA256=""
BINUTILS_SHA256=""
if [ "$GCC_VERSION" = "14.2.0" ];   then GCC_SHA256="a7b39bc69cbf9e25826c5a60ab26477001f7c08d85cec04bc0e29cabed6f3cc9"; fi
if [ "$BINUTILS_VERSION" = "2.43" ]; then BINUTILS_SHA256="b53606f443ac8f01d1d5fc9c39497f2af322d99e14cea5c0b4b124d630379365"; fi

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
    C_RED=$'\033[31m' C_GREEN=$'\033[32m' C_YELLOW=$'\033[33m' C_BLUE=$'\033[34m'
else
    C_RESET= C_RED= C_GREEN= C_YELLOW= C_BLUE=
fi

info()    { printf '%sℹ %s%s\n' "$C_BLUE" "$*" "$C_RESET"; }
success() { printf '%s✔ %s%s\n' "$C_GREEN" "$*" "$C_RESET"; }
warn()    { printf '%s⚠ %s%s\n' "$C_YELLOW" "$*" "$C_RESET"; }
error()   { printf '%s✖ %s%s\n' "$C_RED" "$*" "$C_RESET" >&2; }
die()     { error "$*"; exit 1; }

# Checklist rows; the fixed label width keeps every column aligned
row_ok()   { printf '  %s✔ %-22s%s %s\n' "$C_GREEN" "$1" "$C_RESET" "$2"; }
row_bad()  { printf '  %s✖ %-22s%s %s\n' "$C_RED"   "$1" "$C_RESET" "$2"; }
row_warn() { printf '  %s⚠ %-22s%s %s\n' "$C_YELLOW" "$1" "$C_RESET" "$2"; }

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

# Runs one action of the detected package manager; apt always gets a
# non-interactive frontend so installs and upgrades cannot stop on a prompt.
pkg_run() {
    case "$PKG/$1" in
        apt/update)     DEBIAN_FRONTEND=noninteractive apt-get update ;;
        dnf/update)     dnf makecache ;;
        pacman/update)  pacman -Syu --noconfirm ;; # -Syu = refresh + upgrade, no partial upgrade
        apt/upgrade)    DEBIAN_FRONTEND=noninteractive apt-get upgrade -y ;;
        dnf/upgrade)    dnf upgrade -y ;;
        apt/install)    DEBIAN_FRONTEND=noninteractive apt-get install -y "${PACKAGES_APT[@]}" ;;
        dnf/install)    dnf install -y "${PACKAGES_DNF[@]}" ;;
        pacman/install) pacman -S --noconfirm --needed "${PACKAGES_PACMAN[@]}" ;;
        apt/audio)      DEBIAN_FRONTEND=noninteractive apt-get install -y "${AUDIO_APT[@]}" ;;
        dnf/audio)      dnf install -y "${AUDIO_DNF[@]}" ;;
        pacman/audio)   pacman -S --noconfirm --needed "${AUDIO_PACMAN[@]}" ;;
    esac
}

# Refreshes the package metadata (and optionally upgrades the system)
update_package_index() {
    if [ "$SKIP_UPDATE" = 1 ]; then
        info "Skipping package list refresh (--skip-update)."
        return 0
    fi

    if [ "$PKG" = "pacman" ]; then
        # Refresh and upgrade in one step: installing right after a bare -Sy
        # would leave Arch in an unsupported "partial upgrade" state.
        info "Refreshing package lists and upgrading the system (pacman)..."
        pkg_run update
        success "Package lists refreshed and system upgraded"
        return 0
    fi

    info "Refreshing package lists..."
    pkg_run update
    success "Package lists refreshed"

    if [ "$UPGRADE" = 1 ]; then
        info "Upgrading the system..."
        pkg_run upgrade
        success "System upgraded"
    fi
}

# Installs the packages required by the Makefile and the cross build
install_packages() {
    info "Installing packages..."
    pkg_run install
    success "Packages installed"
}

# Installs the PulseAudio client library QEMU's '-audiodev pa' needs.
# Best effort: a conflict with PipeWire must not abort the whole setup.
install_audio_support() {
    info "Installing PulseAudio client library (QEMU '-audiodev pa')..."
    pkg_run audio ||
        warn "PulseAudio package could not be installed; 'make run' audio may not work."
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

# Downloads a file with curl and verifies its SHA-256 when one is given.
# Finished files are reused; an interrupted download is resumed.
download_file() {
    local url="$1" dest="$2" sha="${3:-}"

    if [ -s "$dest" ]; then
        info "Using cached $(basename "$dest")"
    else
        info "Downloading $url"
        curl -fL --retry 3 --connect-timeout 30 -C - -o "$dest.part" "$url" ||
            # A partial file that cannot be resumed (stale, or no byte-range
            # support) is dropped so the next attempt starts from scratch
            { rm -f "$dest.part"; curl -fL --retry 3 --connect-timeout 30 -o "$dest.part" "$url"; }
        mv "$dest.part" "$dest"
    fi

    if [ -n "$sha" ] && ! printf '%s  %s\n' "$sha" "$dest" | sha256sum -c --status; then
        rm -f "$dest"
        die "Checksum mismatch for $(basename "$dest") — the download may be tampered with or corrupt. It has been deleted; run the script again to re-download it."
    fi
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

# Configures, builds and installs one component into $CROSS_PREFIX, teeing
# all output to the log. Arguments: label, source dir, build dir, log, make
# build goals, make install goals, then the configure flags. Binutils builds
# everything with an empty build goal.
build_component() {
    local label="$1" srcdir="$2" builddir="$3" log="$4" build_tgt="$5" install_tgt="$6"
    shift 6
    if ! (
        cd "$builddir" &&
        "$srcdir/configure" --target=i686-elf --prefix="$CROSS_PREFIX" "$@" &&
        make -j"$JOBS" $build_tgt &&
        make $install_tgt
    ) 2>&1 | tee "$log"; then
        tail -n 20 "$log" >&2 || true
        die "$label build failed (full log: $log)."
    fi
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
        "$src/binutils-$BINUTILS_VERSION.tar.xz" "$BINUTILS_SHA256"
    download_file "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz" \
        "$src/gcc-$GCC_VERSION.tar.xz" "$GCC_SHA256"

    extract_once "$src/binutils-$BINUTILS_VERSION.tar.xz" "$extract" "binutils-$BINUTILS_VERSION"
    extract_once "$src/gcc-$GCC_VERSION.tar.xz" "$extract" "gcc-$GCC_VERSION"

    info "Building binutils (log: $WORK_DIR/build-binutils.log)..."
    build_component binutils "$extract/binutils-$BINUTILS_VERSION" \
        "$WORK_DIR/build-binutils" "$WORK_DIR/build-binutils.log" \
        "" install --disable-nls --disable-werror --disable-gprofng

    info "Building GCC (log: $WORK_DIR/build-gcc.log, this is the slow part)..."
    build_component GCC "$extract/gcc-$GCC_VERSION" \
        "$WORK_DIR/build-gcc" "$WORK_DIR/build-gcc.log" \
        "all-gcc all-target-libgcc" "install-gcc install-target-libgcc" \
        --enable-languages=c --without-headers --disable-multilib --disable-nls \
        --disable-werror --disable-threads --disable-libssp --disable-libgomp \
        --disable-libquadmath

    info "Smoke-testing the new toolchain..."
    if ! cross_compiler_works; then
        die "The cross compiler was built but failed to compile a test file (see logs above)."
    fi

    success "i686-elf cross compiler installed ($(i686-elf-gcc --version | head -n1))"

    # Keep the downloaded tarballs and logs, drop the multi-GB build trees
    rm -rf "$WORK_DIR/build-binutils" "$WORK_DIR/build-gcc" "$extract"
}

# Returns 0 when QEMU can serve '-audiodev pa'. QEMU 8+ loads the backend
# from a module (qemu/audio-pa.so) instead of linking it into the emulator,
# so `ldd` on qemu-system-i386 alone reports a false negative.
qemu_pa_backend() {
    local qemu_bin mod
    qemu_bin="$(command -v qemu-system-i386 2>/dev/null)" || return 1

    # Older builds link libpulse straight into the emulator
    ldd "$qemu_bin" 2>/dev/null | grep -q libpulse && return 0

    for mod in /usr/lib/qemu/audio-pa.so /lib/qemu/audio-pa.so /usr/lib/*/qemu/audio-pa.so; do
        [ -f "$mod" ] || continue
        ldd "$mod" 2>/dev/null | grep -q libpulse && return 0
    done
    return 1
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
            row_ok "$t" "$path"
        else
            row_bad "$t" "missing"
            missing=$((missing + 1))
        fi
    done

    # A cross compiler that cannot compile is as good as missing
    if command -v i686-elf-gcc >/dev/null 2>&1; then
        if cross_compiler_works; then
            row_ok "i686-elf-gcc test" "compiles and links a test file"
        else
            row_bad "i686-elf-gcc test" "installed but failed to compile"
            missing=$((missing + 1))
        fi
    fi

    # grub-mkrescue needs the 32-bit PC modules to build the ISO
    if [ -d /usr/lib/grub/i386-pc ]; then
        row_ok "grub i386-pc modules" "/usr/lib/grub/i386-pc"
    else
        row_bad "grub i386-pc modules" "missing (grub-pc-bin / grub2-pc-modules / grub)"
        missing=$((missing + 1))
    fi

    for t in "${OPTIONAL_TOOLS[@]}"; do
        if path="$(command -v "$t" 2>/dev/null)"; then
            row_ok "$t" "$path (optional)"
        else
            row_warn "$t" "missing (optional)"
        fi
    done

    # 'make run' attaches a Sound Blaster 16 through QEMU's '-audiodev pa'
    if command -v qemu-system-i386 >/dev/null 2>&1 && command -v ldd >/dev/null 2>&1; then
        if qemu_pa_backend; then
            row_ok "qemu pa backend" "PulseAudio backend available"
        else
            row_warn "qemu pa backend" "QEMU has no PulseAudio backend, \"make run\" may fail"
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

# Conflicting flags are rejected before anything else runs
if [ "$SKIP_UPDATE" = 1 ] && [ "$UPGRADE" = 1 ]; then
    die "--skip-update and --upgrade cannot be combined."
fi

printf '%s\n' "======================================" \
    "  OpenKernel Dev Environment Setup" \
    "======================================" ""

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
