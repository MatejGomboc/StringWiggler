# StringWiggler — AI Assistant Context

## What Is This?

StringWiggler is a small, deliberately-pointless cross-platform Vulkan toy: a string dangles
from the mouse cursor and whips around as you move it. C++ with plain C Vulkan via volk.
GPLv3. Repo: <https://github.com/MatejGomboc/StringWiggler> Version 0.1.0.

**Status: WORKING TOY.** The app renders a cyan string that hangs from the mouse cursor and
wiggles: the head is pinned to the cursor and the whole string is simulated on the GPU (Verlet
physics in a Slang compute shader) then drawn as a 128-node line strip via dynamic rendering.
Rendering runs on a dedicated thread that only draws while the string is in motion (sleeping when
settled) and redraws live during resize/move. Runs on any Vulkan 1.3 GPU, including integrated.
See `TODO.md` for what is built and what's left, and `docs/ARCHITECTURE.md` for how the pieces fit
together. Do not duplicate that design state here.

## Critical Rules

### NEVER

1. **NEVER push to `main`** — work on a feature branch and open a PR.
2. **NEVER link Vulkan statically** — load it dynamically via volk with `VK_NO_PROTOTYPES`.
3. **NEVER throw our own exceptions, and never let one cross a public interface** — our
   functions return `bool` and fill a `std::string& out_error_message`. (Test code may throw —
   `TestingLib` assertions do.)
4. **NEVER let a `vk::raii` / vulkan-hpp exception escape a component** — the external Vulkan
   library DOES throw on errors; wrap its calls in `try/catch` inside each `init()` and
   translate to `bool` + `out_error_message`. Exceptions are caught at the boundary, never
   propagated.
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
7. **ALWAYS use vulkan-hpp / `vk::raii` for Vulkan handles and VMA for GPU memory** — RAII
   ownership; Volk loads the entry points and the vulkan-hpp dynamic dispatcher is initialised
   from it (see `instance.cpp`).
8. **ALWAYS keep Vulkan on the render thread** — `main.cpp` pumps window events on the main
   thread and forwards them to the render thread via `SignalsLib::Signal<RenderEvent>` + a
   condition variable; the render thread owns all `drawFrame` / swapchain work. The renderer is
   created on the main thread, used only by the render thread between spawn and join, then
   destroyed after join. Emit render events under the render mutex so a wake-up cannot be lost.

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
│   ├── main.cpp           # Entry point int main(); spawns the render thread, pumps window
│   │                      #   events, forwards them via Signal<RenderEvent> + condvar
│   ├── volk.cpp           # VOLK_IMPLEMENTATION translation unit
│   ├── vma.cpp            # VMA_IMPLEMENTATION translation unit
│   ├── instance.{hpp,cpp} # Engine::Instance — VkInstance + debug messenger; validation
│   │                      #   routed to the logger in debug builds
│   ├── surface.{hpp,cpp}  # createSurface + requiredSurfaceExtensions free functions
│   ├── device.{hpp,cpp}   # Engine::Device — scored selection (prefers discrete, falls back to
│   │                      #   integrated), graphics+compute+present queue, swapchain ext,
│   │                      #   Vulkan 1.3 dynamicRendering + synchronization2
│   ├── allocator.{hpp,cpp}# Engine::Allocator (VMA) + RAII AllocatedBuffer / AllocatedImage
│   ├── swapchain.{hpp,cpp} # Engine::Swapchain — images/views, FIFO present, recreate()
│   ├── pipeline.{hpp,cpp} # Engine::Pipeline — graphics pipeline (line-strip, Vec2 vertex,
│   │                      #   dynamic rendering) built from line.slang
│   ├── compute_pipeline.{hpp,cpp} # Engine::ComputePipeline — physics compute pipeline +
│   │                      #   descriptor set (positions+prev) + PhysicsPush, from physics.slang
│   ├── shader_loader.{hpp,cpp} # executableDirectory() + loadSpirv() (shared by both pipelines)
│   ├── renderer.{hpp,cpp} # Engine::Renderer — composition root: Instance, surface, Device,
│   │                      #   Allocator, Swapchain, Pipeline, ComputePipeline, GPU physics
│   │                      #   buffers + per-frame command/sync. drawFrame = dispatch→barrier→draw
│   ├── line.slang         # vertex + fragment (cyan line) → line.spv
│   ├── physics.slang      # compute (Verlet + constraints + damping) → physics.spv
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
| Vulkan bindings | vulkan-hpp + `vk::raii` (RAII) | Automatic, ordered teardown of handles |
| GPU memory | VMA (Vulkan Memory Allocator) | Standard allocation; fed Volk function pointers |
| Error handling | `bool` + `out_error_message`; catch `vk::raii` throws at the boundary | Our code never throws; external-lib exceptions are contained |
| Logging | `LoggingLib::Logger` → console | Async background thread; console subsystem app (not a log file) |
| Windowing | Abstract `Window` + `create()` factory | Hides Win32/XCB behind `void*` native handles |
| Platforms | Win32 + XCB (X11) only | Matched reach; Wayland via XWayland (macOS/Wayland dropped) |
| Decoupling | C function pointers + `void* user_data` | No `std::function` for cross-component callbacks |
| Rendering | Dynamic rendering + synchronization2 (Vulkan 1.3) | No render pass / framebuffers; `cmd.beginRendering` + `pipelineBarrier2` |
| Physics | GPU compute, Slang `physics.slang` | Per-node Verlet + distance constraints in one workgroup (shared-memory red-black solve) |
| Frame loop | Dedicated render thread, render-on-demand | Draws only while the string moves; sleeps on a condvar when settled; woken by the window `EventCallback` (so resize/move redraw live, even mid modal loop) |
| Present mode | FIFO (v-sync) | Steady physics timestep; low power; integrated-GPU friendly |
| GPU support | Any Vulkan 1.3 device incl. integrated | No RTX / discrete-only features — just a graphics+compute queue + storage buffers |

## Off Limits

- **`LICENCE`** — legal document, do not modify.
