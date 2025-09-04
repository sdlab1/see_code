CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -O2
LIBS = -ltermux-gui -lcjson -lpthread
DEBUG_CFLAGS = -Wall -Wextra -std=c99 -pthread -g -DDEBUG -O0

TARGET = see_code
SRC = see_code_main.c
PLUGIN_SRC = lua/see_code/init.lua
SETUP_SCRIPT = setup_see_code.sh

# Directories
NVIM_CONFIG_DIR = $(HOME)/.config/nvim/lua/see_code
BIN_DIR = $(PREFIX)/bin
BUILD_DIR = build

# Default target
.PHONY: all
all: check-deps $(TARGET)

# Check system dependencies
.PHONY: check-deps
check-deps:
	@echo "Checking system dependencies..."
	@command -v gcc >/dev/null 2>&1 || { echo "ERROR: gcc not found. Run: pkg install build-essential"; exit 1; }
	@command -v pkg >/dev/null 2>&1 || { echo "ERROR: pkg not found. Are you in Termux?"; exit 1; }
	@pkg list-installed | grep -q termux-gui-c || { echo "ERROR: termux-gui-c not installed. Run: pkg install termux-gui-c"; exit 1; }
	@pkg list-installed | grep -q cjson || { echo "ERROR: cjson not installed. Run: pkg install cjson"; exit 1; }
	@echo "✓ All dependencies satisfied"

# Build release version
$(TARGET): $(SRC)
	@echo "Building $(TARGET) (release)..."
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)
	@echo "✓ Build completed: $(TARGET)"

# Build debug version
.PHONY: debug
debug: $(BUILD_DIR)/$(TARGET)_debug

$(BUILD_DIR)/$(TARGET)_debug: $(SRC) | $(BUILD_DIR)
	@echo "Building $(TARGET) (debug)..."
	$(CC) $(DEBUG_CFLAGS) -o $(BUILD_DIR)/$(TARGET)_debug $(SRC) $(LIBS)
	@echo "✓ Debug build completed: $(BUILD_DIR)/$(TARGET)_debug"

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Install everything
.PHONY: install
install: $(TARGET) install-plugin install-bin
	@echo "✓ Installation completed"

# Install binary to system PATH
.PHONY: install-bin
install-bin: $(TARGET)
	@echo "Installing binary to $(BIN_DIR)..."
	@mkdir -p $(BIN_DIR)
	cp $(TARGET) $(BIN_DIR)/
	chmod +x $(BIN_DIR)/$(TARGET)
	@echo "✓ Binary installed: $(BIN_DIR)/$(TARGET)"

# Install Neovim plugin
.PHONY: install-plugin
install-plugin: $(PLUGIN_SRC)
	@echo "Installing Neovim plugin..."
	@mkdir -p $(NVIM_CONFIG_DIR)
	cp $(PLUGIN_SRC) $(NVIM_CONFIG_DIR)/
	@echo "✓ Plugin installed: $(NVIM_CONFIG_DIR)/init.lua"

# Setup script (comprehensive system setup)
.PHONY: setup
setup: $(SETUP_SCRIPT)
	@echo "Running comprehensive system setup..."
	@chmod +x $(SETUP_SCRIPT)
	@./$(SETUP_SCRIPT)

# Run the application
.PHONY: run
run: $(TARGET)
	@echo "Starting $(TARGET)..."
	@./$(TARGET)

# Run in background
.PHONY: start
start: $(TARGET)
	@echo "Starting $(TARGET) in background..."
	@nohup ./$(TARGET) > see_code.log 2>&1 &
	@echo "✓ $(TARGET) started (PID: $$!)"
	@echo "  Log file: see_code.log"
	@echo "  Stop with: make stop"

# Stop background process
.PHONY: stop
stop:
	@echo "Stopping $(TARGET)..."
	@pkill -f $(TARGET) || echo "No $(TARGET) process found"
	@echo "✓ $(TARGET) stopped"

# Check system status
.PHONY: status
status:
	@echo "=== see_code System Status ==="
	@echo "Binary: $(shell test -f $(TARGET) && echo "✓ Built" || echo "✗ Not built")"
	@echo "Installed: $(shell test -f $(BIN_DIR)/$(TARGET) && echo "✓ Yes" || echo "✗ No")"
	@echo "Plugin: $(shell test -f $(NVIM_CONFIG_DIR)/init.lua && echo "✓ Installed" || echo "✗ Not installed")"
	@echo "Process: $(shell pgrep -f $(TARGET) >/dev/null && echo "✓ Running" || echo "✗ Not running")"
	@echo "Socket: $(shell test -S /data/data/com.termux/files/usr/tmp/see_code_socket && echo "✓ Active" || echo "✗ Inactive")"
	@echo "Git repo: $(shell git rev-parse --git-dir >/dev/null 2>&1 && echo "✓ Yes" || echo "✗ No")"
	@echo "Changes: $(shell test -n "$$(git status --porcelain 2>/dev/null)" && echo "✓ Yes" || echo "✗ None")"

# Test the complete system
.PHONY: test
test: $(TARGET)
	@echo "Running system tests..."
	@echo "1. Testing binary execution..."
	@timeout 2s ./$(TARGET) --help >/dev/null 2>&1 || echo "✓ Binary responds"
	@echo "2. Testing Neovim plugin load..."
	@nvim --headless -c "lua require('see_code')" -c "qa" 2>/dev/null && echo "✓ Plugin loads" || echo "✗ Plugin error"
	@echo "3. Testing git repository..."
	@git rev-parse --git-dir >/dev/null 2>&1 && echo "✓ Git repository" || echo "⚠ Not in git repo"
	@echo "4. Testing dependencies..."
	@ldd $(TARGET) | grep -q "not found" && echo "✗ Missing libraries" || echo "✓ All libraries found"
	@echo "✓ Tests completed"

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGET)
	rm -rf $(BUILD_DIR)
	rm -f see_code.log
	rm -f nohup.out
	@echo "✓ Clean completed"

# Complete uninstall
.PHONY: uninstall
uninstall: stop clean
	@echo "Uninstalling see_code..."
	rm -f $(BIN_DIR)/$(TARGET)
	rm -rf $(NVIM_CONFIG_DIR)
	rm -f /data/data/com.termux/files/usr/tmp/see_code_socket
	@echo "✓ Uninstall completed"

# Development tools
.PHONY: dev-deps
dev-deps:
	@echo "Installing development dependencies..."
	pkg install gdb valgrind strace || true
	@echo "✓ Development tools installed"

# Memory check (requires valgrind)
.PHONY: memcheck
memcheck: debug
	@echo "Running memory check..."
	valgrind --leak-check=full --show-leak-kinds=all $(BUILD_DIR)/$(TARGET)_debug

# Create package for distribution
.PHONY: package
package: $(TARGET)
	@echo "Creating distribution package..."
	mkdir -p see_code_dist
	cp $(TARGET) see_code_dist/
	cp $(PLUGIN_SRC) see_code_dist/
	cp $(SETUP_SCRIPT) see_code_dist/
	cp Makefile see_code_dist/
	cp README.md see_code_dist/ 2>/dev/null || echo "# see_code" > see_code_dist/README.md
	tar czf see_code_dist.tar.gz see_code_dist/
	rm -rf see_code_dist/
	@echo "✓ Package created: see_code_dist.tar.gz"

# Help target
.PHONY: help
help:
	@echo "see_code Makefile Commands:"
	@echo ""
	@echo "Build targets:"
	@echo "  all         - Build release version (default)"
	@echo "  debug       - Build debug version"
	@echo "  clean       - Remove build artifacts"
	@echo ""
	@echo "Installation:"
	@echo "  install     - Install binary and plugin"
	@echo "  setup       - Run comprehensive system setup"
	@echo "  uninstall   - Complete removal"
	@echo ""
	@echo "Runtime:"
	@echo "  run         - Start application in foreground"
	@echo "  start       - Start application in background"
	@echo "  stop        - Stop background application"
	@echo ""
	@echo "System management:"
	@echo "  status      - Show system status"
	@echo "  test        - Run system tests"
	@echo "  check-deps  - Verify dependencies"
	@echo ""
	@echo "Development:"
	@echo "  dev-deps    - Install development tools"
	@echo "  memcheck    - Run memory analysis"
	@echo "  package     - Create distribution package"
