```markdown
# see_code

Interactive Git Diff Viewer for Termux using GLES2 or Termux-GUI as a fallback.

## Features

- Visualize Git diffs with file and hunk navigation.
- Communicates with Neovim via a Unix domain socket.
- Uses GLES2 for rendering on Termux:GUI.
- Falls back to Termux-GUI API if GLES2 initialization fails or fonts are unavailable.
- Automatic server startup from Neovim plugin.

## Prerequisites

- **Termux** environment installed on Android.
- **Termux:GUI** application installed (from F-Droid or GitHub).
- Required packages: `freetype`, `mesa`.
- `git` installed in Termux.
- `cmake`, `make`, `clang` for building.
```
Install required packages:
```bash
pkg install freetype mesa cmake make clang git
```

## Installation

Clone or download this repository.
```bash
git clone https://github.com/sdlab1/see_code
```
Navigate to the project root directory.
```bash
cd see_code
```
Make the installation script executable and run it:
```bash
chmod +x scripts/install.sh
./scripts/install.sh
```

This script will:
- Check for dependencies.
- Build the `see_code` C application using CMake.
- Install the `see_code` binary to `$PREFIX/bin`.
- Install the Neovim plugin to the standard plugin directory.

## Usage

1. **Ensure Dependencies:** Make sure you are in a Git repository with uncommitted changes.

2. **Use in Neovim:**
   - Open Neovim in a Git repository.
   - Run the command:
     ```
     :SeeCodeDiff
     ```
   - The first time you run this command, the `see_code` GUI application will be automatically started in the background.
   - The diff data will be sent to the `see_code` GUI application for display.

## Neovim Plugin Commands

- `:SeeCodeDiff` - Send the current Git diff to the GUI (starts GUI if needed).
- `:SeeCodeStatus` - Check the status of dependencies, connection, and server process.

Default keymaps:
- `<Leader>sd` - Send diff (`:SeeCodeDiff`)
- `<Leader>ss` - Check status (`:SeeCodeStatus`)

## Fallback Rendering Sequence

The application attempts to use rendering backends in this order:
1. **FreeType** (for high-quality font rendering) - Tries `/system/fonts/Roboto-Regular.ttf`
2. **TrueType** (system fallback) - Tries `/system/fonts/DroidSansMono.ttf`
3. **Termux API** (basic text rendering if fonts fail) - Uses `libtermux-gui-c` library.
4. **Fail** (if no rendering backend is available)

Fonts used:
- Primary: Roboto-Regular.ttf (FreeType)
- Fallback: DroidSansMono.ttf (TrueType)
- Termux fallback: LiberationMono-Regular.ttf (if available via `pkg install fonts-liberation`)

## Building Manually

If you prefer to build manually:

```bash
# From the project root
mkdir build && cd build
cmake .. # Add -DCMAKE_INSTALL_PREFIX=... if needed
make
# To install
make install
```
