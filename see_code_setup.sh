#!/data/data/com.termux/files/usr/bin/bash

# see_code Setup and Verification Script
# This script sets up the complete see_code system and performs comprehensive checks

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project settings
PROJECT_NAME="see_code"
SOCKET_PATH="/data/data/com.termux/files/usr/tmp/see_code_socket"
NVIM_PLUGIN_DIR="$HOME/.config/nvim/lua/see_code"

echo -e "${BLUE}=== see_code Setup Script ===${NC}"
echo "This script will set up the complete see_code system."
echo ""

# Function to print status
print_status() {
    local status=$1
    local message=$2
    if [ "$status" = "OK" ]; then
        echo -e "${GREEN}✓${NC} $message"
    elif [ "$status" = "WARN" ]; then
        echo -e "${YELLOW}⚠${NC} $message"
    elif [ "$status" = "ERROR" ]; then
        echo -e "${RED}✗${NC} $message"
    else
        echo -e "${BLUE}•${NC} $message"
    fi
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check package installation
check_package() {
    local package=$1
    if dpkg-query -W -f='${Status}' "$package" 2>/dev/null | grep -c "ok installed" >/dev/null; then
        return 0
    else
        return 1
    fi
}

echo -e "${BLUE}Phase 1: System Requirements Check${NC}"
echo "----------------------------------------"

# Check if running in Termux
if [ ! -d "/data/data/com.termux" ]; then
    print_status "ERROR" "Not running in Termux environment"
    echo "SOLUTION: This script must be run inside Termux"
    exit 1
fi
print_status "OK" "Running in Termux environment"

# Check required commands
REQUIRED_COMMANDS=("gcc" "make" "git" "pkg" "nc")
for cmd in "${REQUIRED_COMMANDS[@]}"; do
    if command_exists "$cmd"; then
        print_status "OK" "Command '$cmd' available"
    else
        print_status "ERROR" "Command '$cmd' not found"
        echo "SOLUTION: Run 'pkg install $cmd'"
        exit 1
    fi
done

# Check if Neovim is installed
if command_exists "nvim"; then
    NVIM_VERSION=$(nvim --version | head -n1)
    print_status "OK" "Neovim found: $NVIM_VERSION"
else
    print_status "ERROR" "Neovim not installed"
    echo "SOLUTION: Run 'pkg install neovim'"
    exit 1
fi

echo ""
echo -e "${BLUE}Phase 2: Package Dependencies${NC}"
echo "----------------------------------------"

REQUIRED_PACKAGES=("termux-gui-c" "cjson" "build-essential")
MISSING_PACKAGES=()

for package in "${REQUIRED_PACKAGES[@]}"; do
    if check_package "$package"; then
        print_status "OK" "Package '$package' installed"
    else
        print_status "WARN" "Package '$package' missing"
        MISSING_PACKAGES+=("$package")
    fi
done

# Install missing packages
if [ ${#MISSING_PACKAGES[@]} -gt 0 ]; then
    echo ""
    print_status "INFO" "Installing missing packages: ${MISSING_PACKAGES[*]}"
    pkg install "${MISSING_PACKAGES[@]}" -y
    
    # Verify installation
    for package in "${MISSING_PACKAGES[@]}"; do
        if check_package "$package"; then
            print_status "OK" "Package '$package' installed successfully"
        else
            print_status "ERROR" "Failed to install package '$package'"
            exit 1
        fi
    done
fi

# Check Termux:GUI app installation
echo ""
echo -e "${BLUE}Phase 3: Termux:GUI Application${NC}"
echo "----------------------------------------"

if [ -f "/data/data/com.termux.gui/files/termux-gui" ]; then
    print_status "OK" "Termux:GUI app detected"
else
    print_status "ERROR" "Termux:GUI app not installed"
    echo "SOLUTION: Install Termux:GUI from F-Droid or GitHub releases"
    echo "URL: https://github.com/tareksander/termux-gui/releases"
    exit 1
fi

echo ""
echo -e "${BLUE}Phase 4: Project Compilation${NC}"
echo "----------------------------------------"

# Create project directory if it doesn't exist
if [ ! -d "$PROJECT_NAME" ]; then
    mkdir -p "$PROJECT_NAME"
    print_status "INFO" "Created project directory: $PROJECT_NAME"
fi

cd "$PROJECT_NAME"

# Check if source files exist
if [ ! -f "see_code_main.c" ]; then
    print_status "ERROR" "Source file 'see_code_main.c' not found"
    echo "SOLUTION: Ensure all project files are in the current directory"
    exit 1
fi
print_status "OK" "Source file 'see_code_main.c' found"

# Compile the project
print_status "INFO" "Compiling see_code application..."
if gcc -Wall -Wextra -std=c99 -pthread -o see_code see_code_main.c -ltermux-gui -lcjson -lpthread; then
    print_status "OK" "Compilation successful"
else
    print_status "ERROR" "Compilation failed"
    echo "SOLUTION: Check error messages above and ensure all dependencies are installed"
    exit 1
fi

# Test if binary runs
if ./see_code --help >/dev/null 2>&1 || [ $? -eq 1 ]; then
    print_status "OK" "Binary executable"
else
    print_status "ERROR" "Binary not executable or missing libraries"
    echo "SOLUTION: Check library dependencies with 'ldd see_code'"
    exit 1
fi

echo ""
echo -e "${BLUE}Phase 5: Neovim Plugin Installation${NC}"
echo "----------------------------------------"

# Create Neovim plugin directory
if [ ! -d "$NVIM_PLUGIN_DIR" ]; then
    mkdir -p "$NVIM_PLUGIN_DIR"
    print_status "OK" "Created Neovim plugin directory"
else
    print_status "OK" "Neovim plugin directory exists"
fi

# Copy plugin file
if [ -f "lua/see_code/init.lua" ]; then
    cp lua/see_code/init.lua "$NVIM_PLUGIN_DIR/"
    print_status "OK" "Neovim plugin installed"
else
    print_status "ERROR" "Plugin file 'lua/see_code/init.lua' not found"
    exit 1
fi

# Install binary to system path
cp see_code "$PREFIX/bin/"
print_status "OK" "Binary installed to $PREFIX/bin/"

echo ""
echo -e "${BLUE}Phase 6: System Integration Test${NC}"
echo "----------------------------------------"

# Test git repository
if git rev-parse --git-dir >/dev/null 2>&1; then
    print_status "OK" "Currently in git repository"
    
    # Check for uncommitted changes
    if [ -n "$(git status --porcelain)" ]; then
        print_status "OK" "Git changes detected (good for testing)"
    else
        print_status "WARN" "No git changes found (create some files to test diff)"
    fi
else
    print_status "WARN" "Not in a git repository"
    echo "SOLUTION: Navigate to a git repository or run 'git init' to create one"
fi

# Test socket path access
SOCKET_DIR=$(dirname "$SOCKET_PATH")
if [ -w "$SOCKET_DIR" ]; then
    print_status "OK" "Socket directory writable"
else
    print_status "ERROR" "Cannot write to socket directory: $SOCKET_DIR"
    exit 1
fi

# Test Neovim plugin loading
print_status "INFO" "Testing Neovim plugin loading..."
if nvim --headless -c "lua require('see_code')" -c "qa" 2>/dev/null; then
    print_status "OK" "Neovim plugin loads successfully"
else
    print_status "ERROR" "Neovim plugin failed to load"
    echo "SOLUTION: Check Neovim configuration and plugin path"
    exit 1
fi

echo ""
echo -e "${BLUE}Phase 7: Final System Check${NC}"
echo "----------------------------------------"

# Check if GUI is currently running
if pgrep -f "see_code" >/dev/null; then
    print_status "WARN" "see_code GUI is already running"
    echo "SOLUTION: Stop it with 'pkill see_code' before starting new instance"
else
    print_status "OK" "No conflicting processes found"
fi

# Check available memory
AVAILABLE_MEM=$(free | awk '/^Mem:/{print $7}')
if [ "$AVAILABLE_MEM" -gt 100000 ]; then
    print_status "OK" "Sufficient memory available"
else
    print_status "WARN" "Low memory available ($AVAILABLE_MEM KB)"
fi

echo ""
echo -e "${GREEN}=== Setup Complete ===${NC}"
echo ""
echo "Your see_code system is ready to use!"
echo ""
echo "Quick Start:"
echo "1. Start the GUI:           ./see_code &"
echo "2. Open Neovim in git repo: nvim"
echo "3. Load plugin:             :lua require('see_code').setup()"
echo "4. Check status:            :SeeCodeStatus"
echo "5. Send diff:               :SeeCodeDiff"
echo ""
echo "Key commands:"
echo "  <leader>sd  - Send diff to GUI"
echo "  <leader>ss  - Check system status"
echo "  <leader>sa  - Enable auto-diff on save"
echo ""
echo "Troubleshooting:"
echo "  - If GUI fails to start: Check Termux:GUI app permissions"
echo "  - If connection fails: Verify socket path permissions"
echo "  - If diff is empty: Make sure you have uncommitted changes"
echo ""
print_status "OK" "Setup completed successfully!"
