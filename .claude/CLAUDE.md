# StringWiggler — AI Assistant Context

## What Is This?

StringWiggler is a small, deliberately-pointless cross-platform Vulkan toy: a string dangles
from the mouse cursor and whips around as you move it. C++ with plain C Vulkan via volk.
GPLv3. Repo: <https://github.com/MatejGomboc/StringWiggler> Version 0.1.0.

**Status: EARLY STAGE.** The app opens a window, initialises Vulkan (instance, surface, device
selection), then idles. Nothing is drawn yet — mouse tracking, the string, physics and
rendering are all still to come. See `TODO.md` for what is planned and
`docs/ARCHITECTURE.md` for how the pieces fit together. Do not duplicate that design state here.

## Critical Rules

### NEVER

1. **NEVER push to `main`** — work on a feature branch and open a PR.
2. **NEVER link Vulkan statically** — load it dynamically via volk with `VK_NO_PROTOTYPES`.
3. **NEVER use exceptions in production code** — functions return `bool` and fill a
   `std::string& out_error_message`. (Test code may throw — `TestingLib` assertions do.)
4. **NEVER use vulkan-hpp / `vk::raii`** — plain C Vulkan via volk and `VK_NULL_HANDLE` handles.
5. **NEVER over-engineer** — this is a tiny toy. No abstraction until a second concrete use exists.
6. **NEVER use American spelling** in prose, comments or strings — British English everywhere
   (colour, initialise, behaviour, centre, optimise, recognise, licence).

### ALWAYS

1. **ALWAYS write British spelling** in docs, comments and user-facing strings.
   (Exception: the proper noun "GNU General Public License" keeps its American spelling.)
2. **ALWAYS run `clang-format -i` on changed `.cpp`/`.hpp` files before committing** —
   Allman braces for functions/namespaces, attached for classes/structs/enums, 4-space indent,
   170-column limit, left pointer alignment, `InsertBraces`.
3. **ALWAYS use CMake presets** (`cmake --preset <name>`) — never hand-rolled CMake invocations.
4. **ALWAYS start every source file with the per-file GPLv3 header block.**
5. **ALWAYS free Vulkan handles by explicit `destroy()` in reverse construction order**, guarded
   by an `m_dying`-style flag where relevant. RAII owns the handles.
6. **ALWAYS keep the `libs/` libraries Vulkan-agnostic** — only `src/` touches Vulkan.

## Project Structure

```
StringWiggler/
├── .claude/CLAUDE.md      # This file — repo map (you are here)
├── docs/ARCHITECTURE.md   # Technical architecture (the design source of truth)
├── libs/                  # Vulkan-agnostic internal libraries
│   ├── signals/           # SignalsLib — INTERFACE (header-only). Thread-safe typed FIFO
│   │                      #   Signal<T> (emit/consume). <signal/signal.hpp>
│   ├── testing/           # TestingLib — STATIC. ~250-line unit-test framework:
│   │                      #   TEST_CASE, TEST_CHECK / _EQUAL / _THROWS, runAll().
│   │                      #   Assertions throw → test targets enable exceptions.
│   │                      #   <testing/testing.hpp>
│   ├── logging/           # LoggingLib — STATIC. class Logger: async, thread-safe,
│   │                      #   severity-based (logDebug/Info/Warning/Error/Fatal).
│   │                      #   std::jthread + std::stop_token worker. Writes to CONSOLE
│   │                      #   (Debug/Info → stdout, Warning/Error/Fatal → stderr).
│   │                      #   Depends on signals. <log/logger.hpp>
│   ├── math/              # MathLib — INTERFACE. Vec2/Vec3/Vec4 (string physics uses
│   │                      #   Vec2). <math/vector.hpp>
│   └── window/            # WindowLib — STATIC. Abstract Window + WindowConfig +
│                          #   create(config, logger) factory → unique_ptr<Window>.
│                          #   Tagged-union WindowEvent, internal queue drained by
│                          #   pollEvent() + optional EventCallback. Backends: win32_window,
│                          #   xcb_window. void* nativeHandle()/nativeDisplay() — no platform
│                          #   headers leak. Depends on logging.
├── src/                   # The application — namespace Engine (console subsystem)
│   ├── main.cpp           # Single entry point — int main()
│   ├── volk.cpp           # VOLK_IMPLEMENTATION translation unit
│   ├── instance.{hpp,cpp} # Engine::Instance — VkInstance + debug messenger; validation
│   │                      #   routed to the logger in debug builds
│   ├── surface.{hpp,cpp}  # createSurface + requiredSurfaceExtensions free functions
│   ├── device.{hpp,cpp}   # Engine::Device — scored physical-device selection (prefers
│   │                      #   discrete GPUs), graphics+present queues, swapchain ext, logical device
│   ├── renderer.{hpp,cpp} # Engine::Renderer — composition root owning Instance + surface + Device
│   ├── native_window_handle.hpp
│   └── vulkan_helpers.hpp
├── CMakeLists.txt / CMakePresets.json
├── LICENCE                # GPLv3 (British-spelt filename) — OFF LIMITS
├── README.md  TODO.md  CONTRIBUTING.md  SECURITY.md  CODE_OF_CONDUCT.md  CHANGELOG.md
└── tsan_suppressions.txt
```

## Building

Requires CMake >= 3.16, Ninja Multi-Config, Vulkan SDK 1.3+ (loaded via volk).

```bash
# Windows
cmake --preset windows-msvc
cmake --build build/windows-msvc --config Debug
cmake --workflow --preset=windows-msvc   # one-shot configure+build+test

# Linux (needs the XCB dev package: sudo apt-get install libxcb1-dev)
cmake --preset linux-gcc
cmake --build build/linux-gcc --config Debug

# Tests
ctest --preset windows-msvc-debug
```

Configure presets: `linux-gcc`, `linux-clang`, `linux-clang-asan`, `linux-clang-tsan`,
`windows-msvc`, `windows-clang-cl`, `windows-mingw`. Build/test presets are
`<configure>-debug` / `<configure>-release` (sanitiser presets are debug-only).
Warnings are errors on every compiler.

## Key Design Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| Code layout | Modular `libs/` + `src/` app, wired by CMake | Self-contained, Vulkan-agnostic bricks |
| Vulkan loading | volk, dynamic, `VK_NO_PROTOTYPES` | No static link; pulls 1.3 entry points at runtime |
| Vulkan bindings | Plain C + `VK_NULL_HANDLE` | No vulkan-hpp / `vk::raii` dependency for a toy |
| Error handling | `bool` + `std::string& out_error_message` | No exceptions in production code |
| Logging | `LoggingLib::Logger` → console | Async background thread; console subsystem app (not a log file) |
| Windowing | Abstract `Window` + `create()` factory | Hides Win32/XCB behind `void*` native handles |
| Platforms | Win32 + XCB (X11) only | Matched reach; Wayland via XWayland (macOS/Wayland dropped) |
| Decoupling | C function pointers + `void* user_data` | No `std::function` for cross-component callbacks |

## Off Limits

- **`LICENCE`** — legal document, do not modify.
