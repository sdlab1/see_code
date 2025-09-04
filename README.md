# see_code

Interactive Git Diff Viewer for Termux using GLES2.

This application provides a graphical interface to visualize Git diffs directly from Neovim on Android via Termux.

## Features

- Visualize Git diffs with file and hunk navigation.
- Communicates with Neovim via a Unix domain socket.
- Uses GLES2 for rendering on Termux:GUI.

## Prerequisites

- **Termux** environment installed on Android.
- **Termux:GUI** application installed (from F-Droid or GitHub).
- Required packages: `cjson`, `freetype`, `mesa`.
- `git` installed in Termux.
- `cmake`, `make`, `gcc` for building.

Install required packages:
```bash
pkg install cjson freetype mesa cmake make gcc git
