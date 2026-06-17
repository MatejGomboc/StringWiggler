# Development Environment Setup

How to set up a StringWiggler build environment from scratch. StringWiggler is a tiny Vulkan toy,
so the toolchain is small.

---

## Prerequisites

| Tool | Minimum version | Download |
|------|-----------------|----------|
| CMake | 3.16 | <https://cmake.org/download/> |
| Ninja | any recent | <https://ninja-build.org/> (bundled with most IDEs and the VS C++ workload) |
| C++20 compiler | see below | platform-specific |
| Vulkan SDK | 1.4.335.0 | <https://vulkan.lunarg.com/sdk/home> |
| Git | any recent | <https://git-scm.com/downloads> |

The build uses **Ninja Multi-Config** and CMake presets. Vulkan is loaded dynamically through volk
(`VK_NO_PROTOTYPES`); a Vulkan 1.3+ capable driver is required to actually run the application.

### Compiler (pick one per platform)

**Windows:**

| Compiler | Preset | Notes |
|----------|--------|-------|
| MSVC (Visual Studio 2022+) | `windows-msvc` | Recommended. Install the "Desktop development with C++" workload |
| Clang-CL (LLVM for Windows) | `windows-clang-cl` | Needs LLVM installed alongside MSVC |
| MinGW-w64 (GCC for Windows) | `windows-mingw` | Install via MSYS2 or standalone |

**Linux:**

| Compiler | Preset | Notes |
|----------|--------|-------|
| GCC 12+ | `linux-gcc` | `sudo apt install g++` |
| Clang 15+ | `linux-clang` | `sudo apt install clang` |

---

## Windows quick-start

1. Install Visual Studio 2022 (or the Build Tools) with the **"Desktop development with C++"**
   workload, which provides MSVC, CMake and Ninja.
2. Install the Vulkan SDK from <https://vulkan.lunarg.com/sdk/home>. The installer sets the
   `VULKAN_SDK` environment variable; restart your terminal afterwards. Verify with
   `echo %VULKAN_SDK%` (it should print a path such as `C:\VulkanSDK\1.4.335.0`).
3. Clone, configure and build:

```bash
git clone https://github.com/MatejGomboc/StringWiggler.git
cd StringWiggler
cmake --preset windows-msvc
cmake --build build/windows-msvc --config Debug
```

4. Run the tests:

```bash
ctest --preset windows-msvc-debug
```

5. Run the application:

```bash
build\windows-msvc\src\Debug\StringWiggler.exe
```

At this early stage the application opens a window, initialises Vulkan and idles. Close the window
to exit.

For Clang-CL or MinGW substitute `windows-clang-cl` or `windows-mingw` for `windows-msvc` above.

---

## Linux quick-start (Ubuntu/Debian)

1. Install the toolchain and the XCB development headers (needed for the X11/XCB window backend):

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git libxcb1-dev
# for Clang instead of GCC:
sudo apt install -y clang
```

2. Install the Vulkan SDK following the instructions at <https://vulkan.lunarg.com/sdk/home>. Verify
   with `echo $VULKAN_SDK` and `vulkaninfo --summary`.

3. Clone, configure and build:

```bash
git clone https://github.com/MatejGomboc/StringWiggler.git
cd StringWiggler
cmake --preset linux-gcc
cmake --build build/linux-gcc --config Debug
```

4. Run the tests and the application:

```bash
ctest --preset linux-gcc-debug
./build/linux-gcc/src/Debug/StringWiggler
```

Use `linux-clang` instead of `linux-gcc` for the Clang toolchain. Wayland sessions run the X11/XCB
window via XWayland.

---

## One-shot workflow

Each configure preset has a matching workflow preset that does configure → build (Debug + Release)
→ test in a single command:

```bash
cmake --workflow --preset=windows-msvc
cmake --workflow --preset=linux-gcc
```

There are also Clang sanitiser configure presets on Linux, `linux-clang-asan` (ASan + UBSan) and
`linux-clang-tsan` (TSan), for chasing memory and threading bugs.

---

## Troubleshooting

### `VULKAN_SDK` not found

CMake reports `Could NOT find Vulkan`. Make sure the Vulkan SDK is installed and the `VULKAN_SDK`
environment variable is set, then **restart your terminal** so the new variable is picked up.

### `libxcb` not found (Linux)

A build error such as `xcb/xcb.h: No such file` means the XCB development headers are missing:

```bash
sudo apt install -y libxcb1-dev
```

### Warnings treated as errors

The project builds with warnings-as-errors (`/WX` on MSVC, `-Werror` otherwise). If a warning stops
the build, fix the warning rather than disabling the flag.

---

*See [CONTRIBUTING.md](../CONTRIBUTING.md) for coding style and workflow, and
[ARCHITECTURE.md](ARCHITECTURE.md) for how the code is laid out.*
