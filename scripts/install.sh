#!/data/data/com.termux/files/usr/bin/bash

# scripts/install.sh
set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"

print_status() {
    local status=$1
    local message=$2
    case "$status" in
        OK) echo -e "${GREEN}✓${NC} $message" ;;
        WARN) echo -e "${YELLOW}⚠${NC} $message" ;;
        ERROR) echo -e "${RED}✗${NC} $message" ;;
        INFO) echo -e "${BLUE}•${NC} $message" ;;
    esac
}

check_termux() {
    if [[ ! -d "/data/data/com.termux" ]]; then
        print_status "ERROR" "This script must be run inside Termux."
        exit 1
    fi
    print_status "OK" "Running in Termux environment."
}

check_deps() {
    local missing=()
    for dep in gcc g++ cmake make pkg git; do
        if ! command -v "$dep" &> /dev/null; then
            missing+=("$dep")
        fi
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        print_status "ERROR" "Missing dependencies: ${missing[*]}"
        echo "Run: pkg install ${missing[*]}"
        exit 1
    fi
    print_status "OK" "Build dependencies found."
}

check_packages() {
    local required_packages=("cjson" "freetype" "mesa")
    local missing_packages=()
    for pkg in "${required_packages[@]}"; do
        if ! dpkg -l | grep -q "^ii  $pkg "; then
            missing_packages+=("$pkg")
        fi
    done
    if [[ ${#missing_packages[@]} -gt 0 ]]; then
        print_status "WARN" "Installing missing packages: ${missing_packages[*]}"
        pkg install "${missing_packages[@]}" -y
    fi
    print_status "OK" "Required system packages checked/installed."
}

check_termux_gui_app() {
    if [[ ! -d "/data/data/com.termux.gui" ]]; then
        print_status "ERROR" "Termux:GUI app is not installed."
        echo "Please install it from F-Droid or GitHub releases."
        exit 1
    fi
    print_status "OK" "Termux:GUI app detected."
}

build_project() {
    cd "$PROJECT_ROOT"
    if [[ -d "build" ]]; then
        rm -rf build
    fi
    mkdir build && cd build
    cmake .. -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
    make -j$(nproc)
    print_status "OK" "Project built successfully."
}

install_files() {
    cd "$PROJECT_ROOT/build"
    make install
    print_status "OK" "see_code binary and Neovim plugin installed."
}

main() {
    echo -e "${BLUE}=== see_code Installation Script ===${NC}"
    check_termux
    check_deps
    check_packages
    check_termux_gui_app
    build_project
    install_files
    echo -e "${GREEN}=== Installation Complete ===${NC}"
    echo "Run 'see_code' to start the GUI application."
    echo "In a git repository in Neovim, run ':SeeCodeDiff' to send changes."
}

main "$@"
