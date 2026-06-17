# 〰️ StringWiggler

A cross-platform simulator of a wiggling string. Move your mouse cursor around the screen and watch a string dangle from it, whipping around as you go. A simple, satisfying, utterly pointless app for when you have nothing better to do.

Built with Vulkan for rendering.

**Status:** Early stage: opens a window, initialises Vulkan, then idles — the string, mouse tracking and physics are still to come.

## Platforms

| Platform | Windowing | Status |
|----------|-----------|--------|
| Windows  | Win32     | Active |
| Linux    | X11 (XCB) | Active |

macOS and Wayland are not currently supported (Wayland users can rely on XWayland).

## Requirements

| Requirement    | Version             |
|----------------|---------------------|
| Vulkan SDK     | 1.3+                |
| C++20 compiler | MSVC, GCC, or Clang |
| CMake          | 3.16+               |
| Ninja          | any recent          |

## Building

Quick start (prerequisites already installed):

```bash
# Windows (MSVC)
cmake --preset windows-msvc
cmake --build build/windows-msvc --config Debug

# Linux (GCC) — needs libxcb1-dev
sudo apt-get install libxcb1-dev
cmake --workflow --preset=linux-gcc
```

Full setup instructions are in [docs/DEV_ENV_SETUP.md](docs/DEV_ENV_SETUP.md).

## Running tests

```bash
ctest --preset windows-msvc-debug   # or linux-gcc-debug
```

## Documentation

| Document                                     | Purpose                                  |
|----------------------------------------------|------------------------------------------|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Technical architecture and module layout |
| [CONTRIBUTING.md](CONTRIBUTING.md)           | Contributor guidelines                   |
| [STYLE.md](STYLE.md)                         | Code style conventions                   |
| [SECURITY.md](SECURITY.md)                   | Reporting security issues                |
| [TODO.md](TODO.md)                           | Active tasks and development journal     |

## Contributing

Contributions welcome — see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Security

Found a security issue? Please see [SECURITY.md](SECURITY.md).

## Licence

Copyright (C) 2025 Matej Gomboc <https://github.com/MatejGomboc/StringWiggler>.

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

GNU General Public License v3.0 — see the [LICENCE](LICENCE) file (British spelling) for the full text.
