#!/data/data/com.termux/files/usr/bin/bash

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
NVIM_CONFIG_DIR="${HOME}/.config/nvim"

print_status() {
    local status=$1
    local message=$2
    case "$status" in
        OK)    echo -e "[\033[0;32m OK \033[0m] $message" ;;
        WARN)  echo -e "[\033[1;33mWARN\033[0m] $message" ;;
        ERROR) echo -e "[\033[0;31mERROR\033[0m] $message" ;;
        INFO)  echo -e "[\033[0;34mINFO\033[0m] $message" ;;
        *)     echo -e "[\033[0;35m  ?? \033[0m] $message" ;;
    esac
}

check_termux() {
    if [[ ! -d "/data/data/com.termux" ]]; then
        print_status "ERROR" "This script must be run inside Termux."
        exit 1
    fi
    print_status "OK" "Running in Termux environment."
}

# --- CHANGED: Simplified build deps check, removed cjson/lua checks ---
check_build_deps() {
    local missing=()
    for dep in clang make cmake pkg-config git; do
        if ! command -v "$dep" &> /dev/null; then
            missing+=("$dep")
        fi
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        print_status "ERROR" "Missing build dependencies: ${missing[*]}"
        echo "Attempting to install them..."
        pkg install "${missing[@]}" -y || { print_status "ERROR" "Failed to install build dependencies."; exit 1; }
    fi
    print_status "OK" "Build dependencies found or installed."
}
# --- END CHANGE ---

# --- CHANGED: Removed cjson installation logic ---
check_runtime_packages() {
    local required_packages=("freetype" "mesa" "termux-gui-c")
    local missing_packages=()
    for pkg in "${required_packages[@]}"; do
        if ! dpkg -l | grep -q "^ii  $pkg "; then
            missing_packages+=("$pkg")
        fi
    done

    if [[ ${#missing_packages[@]} -gt 0 ]]; then
        print_status "INFO" "Installing missing runtime packages: ${missing_packages[*]}"
        pkg install "${missing_packages[@]}" -y || { print_status "ERROR" "Failed to install some runtime packages."; exit 1; }
        print_status "OK" "Runtime packages installed."
    else
        print_status "OK" "Required runtime packages are installed."
    fi
}
# --- END CHANGE ---

check_termux_gui_app() {
    if [[ ! -d "/data/data/com.termux.gui" ]]; then
        print_status "ERROR" "Termux:GUI app is not installed."
        echo "Please install it from F-Droid or GitHub releases."
        echo "URL: https://github.com/termux/termux-gui/releases"
        exit 1
    fi
    print_status "OK" "Termux:GUI app detected."
}

build_project() {
    cd "$PROJECT_ROOT"
    if [[ -d "build" ]]; then
        print_status "INFO" "Removing old build directory."
        rm -rf build
    fi
    print_status "INFO" "Creating build directory and running CMake."
    mkdir build && cd build
    if cmake .. -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" -DCMAKE_C_COMPILER=clang; then
        print_status "INFO" "Starting build process with $(nproc) jobs."
        if make -j$(nproc); then
            print_status "OK" "Project built successfully."
            return 0
        else
            print_status "ERROR" "Build process failed."
            return 1
        fi
    else
        print_status "ERROR" "CMake configuration failed."
        return 1
    fi
}

install_files() {
    cd "$PROJECT_ROOT/build"
    print_status "INFO" "Installing files to $INSTALL_PREFIX."
    if make install; then
        print_status "OK" "see_code binary and Neovim plugin installed."
        return 0
    else
        print_status "ERROR" "Installation of files failed."
        return 1
    fi
}

create_config_example() {
    local config_example_dir="$NVIM_CONFIG_DIR"
    local config_example_file="$config_example_dir/see_code_config.lua.example"

    mkdir -p "$config_example_dir"

    if [[ ! -f "$config_example_file" ]]; then
        print_status "INFO" "Creating example configuration file."
        cat > "$config_example_file" << 'EOF'
return {
    socket_path = "/data/data/com.termux/files/usr/tmp/see_code_socket",
    see_code_binary = "see_code",
    font_chain = {
        "/system/fonts/Roboto-Regular.ttf",
        "/system/fonts/DroidSansMono.ttf",
        "/data/data/com.termux/files/usr/share/fonts/liberation/LiberationMono-Regular.ttf"
    },
    fallback_behavior = "termux_gui",
    auto_start_server = true,
    verbose = false
}
EOF
        print_status "OK" "Example configuration file created at $config_example_file"
        echo "Review it and copy it to ~/.config/nvim/see_code_config.lua if you want to customize settings."
    else
        print_status "INFO" "Example configuration file already exists at $config_example_file"
    fi
}

main() {
    echo -e "\033[1;36m=== see_code Installation Script ===\033[0m"
    check_termux
    check_build_deps # This no longer checks for cjson
    check_runtime_packages # This no longer tries to install cjson
    check_termux_gui_app
    if ! build_project; then
        print_status "ERROR" "Build failed. Check the output above."
        exit 1
    fi
    if ! install_files; then
        print_status "ERROR" "Installation failed. Check the output above."
        exit 1
    fi
    create_config_example
    echo -e "\033[0;32m=== Installation Complete ===\033[0m"
    echo "Run 'see_code' to start the GUI application manually."
    echo "Or use ':SeeCodeDiff' in Neovim."
    echo "An example configuration file has been placed in your Neovim config directory."
}

main "$@"
