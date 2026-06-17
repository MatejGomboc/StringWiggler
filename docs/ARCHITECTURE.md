# Architecture

Technical architecture of StringWiggler.

> StringWiggler is a deliberately-pointless Vulkan toy: a string dangles from the mouse cursor
> and whips around as you move it. This document describes the real structure of the codebase as
> built. See [TODO.md](../TODO.md) for the development journal.

---

## Overview

StringWiggler is a tiny cross-platform application written in C++20 on top of Vulkan. It is split
into a handful of small, self-contained libraries under `libs/` and a thin application under `src/`
(the `Engine` namespace). The libraries are general-purpose building blocks; the application wires
them together and owns the Vulkan back end.

The application opens a window and renders a cyan string that hangs from the mouse cursor and
wiggles. The head node is pinned to the cursor; the whole string is simulated on the GPU by a Slang
compute shader (Verlet integration + distance constraints) and drawn as a 128-node line strip with
dynamic rendering. The frame loop runs on a dedicated render thread that draws only while the string
is in motion and redraws live during window resize/move. It runs on any Vulkan 1.3 GPU, integrated
included — there are no ray-tracing or discrete-only requirements.

External dependencies are minimal: the Vulkan SDK, volk (the dynamic Vulkan loader), vulkan-hpp with
`vk::raii` (RAII C++ bindings, header-only, from the SDK), and VMA (the Vulkan Memory Allocator, also
from the SDK). Shaders are written in Slang and compiled to SPIR-V by `slangc` (validated with
`spirv-val`) at build time. Vulkan is loaded dynamically (`VK_NO_PROTOTYPES`; the vulkan-hpp dynamic
dispatcher is fed from volk). Vulkan 1.3 or newer is required (dynamic rendering + synchronization2).

```text
┌──────────────────────────────────────────────────────────────┐
│                    Application  (src/, Engine)                 │
│  main.cpp — window events (main thread) + render thread        │
│  Renderer ── Instance · surface · Device · Allocator           │
│           ── Swapchain · Pipeline (graphics) · ComputePipeline │
│  Shaders: physics.slang (compute) + line.slang (vertex/frag)   │
├──────────────────────────────────────────────────────────────┤
│                       Libraries  (libs/)                       │
│   window  (Win32 / XCB)        math  (Vec2/Vec3/Vec4)         │
│   logging  (async console logger)   signals  (Signal<T> FIFO) │
│   testing  (unit-test framework — test targets only)         │
├──────────────────────────────────────────────────────────────┤
│          volk  (dynamic loader) + VMA + vulkan-hpp            │
│  Resolves Vulkan function pointers at runtime                 │
└──────────────────────────────────────────────────────────────┘
```

---

## Layering and Dependencies

The libraries form a small dependency stack. Nothing reaches upward; each layer only knows about
the ones below it.

```text
        app (src/, Engine)
        │      │      │      │
        ▼      ▼      ▼      ▼
     window  logging  math  signals
        │      │
        ▼      ▼
     logging  signals
        │
        ▼
     signals
```

- **`libs/signals`** — INTERFACE (header-only), namespace `SignalsLib`. A thread-safe typed FIFO
  queue `Signal<T>` with `emit()` / `consume()`. Header: `<signal/signal.hpp>`. No dependencies.
- **`libs/logging`** — STATIC, namespace `LoggingLib`. The `Logger` class (see below). Depends on
  `signals` (it uses `Signal<LogMessage>` as its internal queue). Header: `<log/logger.hpp>`.
- **`libs/math`** — INTERFACE, namespace `MathLib`. `Vec2` / `Vec3` / `Vec4`. The string nodes are
  `Vec2` (shared with the compute shader's vertex/storage layout). Header: `<math/vector.hpp>`. No
  dependencies.
- **`libs/window`** — STATIC, namespace `WindowLib`. The window abstraction and platform backends.
  Depends on `logging`. Header: `<window/window.hpp>`.
- **`libs/testing`** — STATIC, namespace `TestingLib`. An in-house unit-test framework (~250 lines):
  `TEST_CASE` auto-registration, `TEST_CHECK` / `TEST_CHECK_EQUAL` / `TEST_CHECK_THROWS`, and
  `runAll()`. Header: `<testing/testing.hpp>`. Linked only by test targets, never by the application.
- **`src/` (the application)**, namespace `Engine`. Depends on `window`, `logging`, `math` and
  `signals` (the render thread consumes a `Signal<RenderEvent>` queue fed by the main thread). Owns
  the Vulkan back end through `Renderer`.

Library namespaces are PascalCase with a `Lib` suffix; the application uses `Engine`. Each library
has its own `include/<name>/` directory, its own `tests/` directory linking `testing`, and a plain
CMake target name (`signals`, `logging`, `math`, `window`, `testing`).

---

## Key Design Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| Vulkan loading | volk (dynamic) | No static link; `VK_NO_PROTOTYPES` defined globally |
| Vulkan bindings | vulkan-hpp + `vk::raii` (RAII) | Automatic, ordered handle teardown |
| GPU memory | VMA (Vulkan Memory Allocator) | Standard allocation; fed volk function pointers |
| Vulkan version | 1.3+ required | Modern baseline; dynamic rendering available later |
| Error handling | `bool` + `out_error_message`; `vk::raii` throws caught at the boundary | Our code never throws; external-lib exceptions are contained |
| Cross-component callbacks | C function pointers + `void* user_data` | Simple, debuggable, no `std::function` |
| Resource ownership | RAII; explicit `destroy()` in reverse order for Vulkan handles | Predictable teardown |
| Inter-thread messaging | `SignalsLib::Signal<T>` | One thread-safe queue type, reused by the logger |
| Logging | Async, severity-based, to console | Console-subsystem app; Debug/Info → stdout, the rest → stderr |
| Window backends | Win32 and XCB only | Matches the platforms we actually support |
| Native handles | Exposed as `void*` | Consumers never include platform headers |
| Rendering | Dynamic rendering + synchronization2 (Vulkan 1.3) | No render pass / framebuffers; `beginRendering` + `pipelineBarrier2` |
| Shaders | Slang → SPIR-V via `slangc` (validated by `spirv-val`) | One source per stage set; entry points selected per pipeline stage |
| Physics | GPU compute (`physics.slang`) | Per-node Verlet + distance constraints in one workgroup (shared-memory red-black solve) |
| Frame loop | Dedicated render thread; render-on-demand | Draws only while the string moves; sleeps on a condvar when settled |
| Present mode | FIFO (v-sync) | Steady physics timestep; low power; integrated-GPU friendly |
| Spelling | British English in prose/comments/strings | Repo standard (colour, initialise, behaviour) |

---

## Component Model

### Logger (`libs/logging`) *(built)*

`LoggingLib::Logger` is asynchronous and thread-safe. Any thread may call the typed methods
`logDebug` / `logInfo` / `logWarning` / `logError` / `logFatal`; these push a `LogMessage`
(`Severity` + text) onto an internal `SignalsLib::Signal<LogMessage>` queue and return immediately.
A background worker drains the queue and writes to the console: `Debug` and `Info` go to **stdout**,
`Warning` / `Error` / `Fatal` go to **stderr**. (`logFatal` writes straight to stderr synchronously.)

The worker runs on a `std::jthread` and is woken by a `std::condition_variable_any`. The thread's
`std::stop_token` and the auto-join behaviour of `std::jthread` make shutdown RAII: when the `Logger`
is destroyed, stop is requested, the worker wakes, drains and exits, and the destructor joins it.

### Window (`libs/window`) *(built)*

`WindowLib::Window` is an abstract base class. A free factory

```cpp
std::unique_ptr<Window> create(const WindowConfig& config, LoggingLib::Logger& logger);
```

returns the platform-appropriate concrete window (Win32 or XCB) so callers never include platform
headers. `WindowConfig` carries the title, initial size and a couple of flags.

Events are delivered through a platform-neutral tagged union, `WindowEvent` (a `Type` discriminator
plus a union of resize / key / mouse-move / mouse-button payloads). Backends call `pumpEvents()` or
`waitEvents()` to translate native messages into `WindowEvent`s, which are pushed onto an internal
queue and drained by `pollEvent()`. An optional immediate `EventCallback`

```cpp
using EventCallback = void (*)(const WindowEvent& ev, void* user_data);
```

is invoked *in addition* to the queue, so a caller can react during modal operations (such as a
Win32 resize drag) when the main loop is otherwise blocked. Native handles are exposed generically
as `void* nativeHandle()` and `void* nativeDisplay()`.

### Vulkan back end (`src/`, `Engine`) *(built)*

`Engine::Renderer` is the composition root for Vulkan. It owns, in dependency order:

```text
Renderer
  ├── Instance         (VkInstance + debug messenger)
  ├── surface          (VkSurfaceKHR — owned directly by Renderer)
  ├── Device           (physical + logical device; graphics+compute & present queues)
  ├── Allocator        (VMA allocator + RAII AllocatedBuffer / AllocatedImage)
  ├── positions / prev (GPU storage buffers — the string's Verlet state)
  ├── Swapchain        (images + views; FIFO present)
  ├── Pipeline         (graphics: line-strip, one Vec2 vertex attribute)
  ├── ComputePipeline  (physics: descriptor set + PhysicsPush push constants)
  └── command pool + per-frame command buffer + sync (one frame in flight)
```

- **`Engine::Instance`** initialises volk, creates the `VkInstance` and, in debug builds, a
  `VkDebugUtilsMessengerEXT` whose validation output is routed to the `Logger`.
- **`surface.hpp`** provides two free functions: `requiredSurfaceExtensions()` (the WSI extensions
  for the current platform) and `createSurface()` (builds a `VkSurfaceKHR` from a
  `NativeWindowHandle`). Keeping both together concentrates all platform WSI knowledge in one file.
- **`Engine::Device`** scores the available physical devices, preferring discrete but falling back
  to integrated, and requires a combined **graphics + compute** queue family, a present queue and
  the `VK_KHR_swapchain` extension. It enables the Vulkan 1.3 `dynamicRendering` and
  `synchronization2` features on the logical device.
- **`Engine::Allocator`** wraps VMA (fed volk's function pointers) and hands out RAII
  `AllocatedBuffer` / `AllocatedImage` values.
- **`Engine::Swapchain`** picks an sRGB format and **FIFO** present mode, creates the images and
  views, and recreates itself on resize / out-of-date.
- **`Engine::Pipeline`** is the graphics pipeline (line-strip topology, one `Vec2` vertex attribute,
  dynamic viewport/scissor, dynamic rendering) built from `line.slang`.
- **`Engine::ComputePipeline`** is the physics pipeline: a descriptor set binding the two storage
  buffers (current + previous positions) and a `PhysicsPush` push-constant block, built from
  `physics.slang`. `shader_loader` provides the shared `loadSpirv()` / `executableDirectory()`.

`main.cpp` (`int main()`, console subsystem) constructs the `Logger`, creates the `Window`,
initialises the `Renderer`, then spawns the **render thread** and runs the window event loop on the
main thread. See *Frame loop and physics* and *Threading* below.

---

## Frame loop and physics

Each frame, on the render thread, the renderer records one command buffer that does both the
simulation and the draw:

1. **Dispatch** the physics compute shader (`physics.slang`). One workgroup of 128 threads — one
   per node — runs Verlet integration with gravity, pins the head node to the cursor, then relaxes
   the distance constraints between adjacent nodes with even/odd (red-black) Gauss-Seidel passes in
   shared memory, synchronised by `GroupMemoryBarrierWithGroupSync`. Cursor position, delta time,
   gravity, damping, segment length and the iteration count arrive as push constants.
2. **Barrier** — a `pipelineBarrier2` makes the compute shader's writes to the positions buffer
   visible to the vertex stage (the buffer is bound as both a storage buffer and the vertex buffer).
3. **Draw** — transition the swapchain image to colour-attachment, `beginRendering`, draw the
   positions buffer as a 128-vertex line strip, `endRendering`, transition to present.

The string's state lives in two GPU storage buffers (current + previous positions). Because they
are shared between the compute and graphics stages there is exactly **one frame in flight**, which
removes any cross-frame data race and is plenty at v-sync. The cursor is mapped from window client
pixels to NDC (Vulkan clip space is +Y down, matching screen pixels, so no flip is needed).

---

## Error Handling

Our own code never throws. Operations that can fail return `bool` and write a human-readable reason
into a `std::string& out_error_message` out-parameter, for example:

```cpp
bool Renderer::init(LoggingLib::Logger& logger, const NativeWindowHandle& window_handle, std::string& out_error_message);
```

The caller logs the message and exits. The external Vulkan library (`vk::raii` / vulkan-hpp) does throw
`vk::SystemError` on Vulkan errors; each component's `init()` wraps those calls in `try/catch` and
translates them to `out_error_message`, so no exception escapes our interfaces. Test code is the other
place exceptions are allowed: `TestingLib` assertions throw, so test targets are built with exceptions
enabled.

Resource ownership is RAII. Vulkan handles are released by an explicit `destroy()` on each owner,
called in reverse construction order (Device → surface → Instance), and each `destroy()` is safe to
call more than once.

---

## Threading

The application runs three threads:

- **Main thread** — owns the window. It pumps native events (`waitEvents()`, blocking when idle)
  and watches for the close request. It does not touch Vulkan after start-up.
- **Render thread** — owns the entire frame loop and all Vulkan work after `init`. It renders while
  the string is in motion (a short settle window after the last event) and otherwise sleeps on a
  `std::condition_variable`. This is render-on-demand: an idle, settled window costs no CPU/GPU.
- **Logger thread** — the `Logger`'s `std::jthread` worker draining the log queue (as above).

The main thread forwards window events to the render thread through a
`SignalsLib::Signal<RenderEvent>` queue plus the condition variable. Crucially, the window's
**immediate `EventCallback`** runs on the main/UI thread *even during the Win32 modal resize/move
loop* (when the main loop is blocked inside the OS), so the render thread is still woken and the
window keeps redrawing **live** while being dragged. Events are emitted **under the render mutex**
(the same one the render thread waits on), so a wake-up can never be lost. The renderer is created
on the main thread, used only by the render thread between spawn and join, then destroyed on the
main thread after the join — so its Vulkan objects are never touched by two threads at once.
`SignalsLib::Signal<T>` guards its own queue with a mutex, so emit/consume are independently
thread-safe.

---

## Shutdown

Teardown is ordered and idempotent:

- On close, the main loop emits a `Stop` event (under the render mutex) and notifies the render
  thread, then `join()`s it. The window `EventCallback` is cleared next, so no late event can touch
  freed state during window destruction.
- The `Renderer` (destroyed on the main thread after the join) `waitIdle()`s the device and calls
  `destroy()` on its members in reverse construction order; each Vulkan owner guards against a
  second call, so a manual `destroy()` followed by the destructor is harmless.
- The `Logger` is destroyed last in `main`. Its `std::jthread` requests stop via the `std::stop_token`,
  the worker drains any remaining messages, and the destructor joins the thread.

There is no elaborate multi-phase shutdown protocol — a Stop/join handshake plus RAII and
reverse-order `destroy()` is enough for a toy of this size.

---

## Future ideas

The drawing pipeline and physics are built: swapchain, graphics + compute pipelines, mouse
tracking, GPU Verlet physics, and the threaded render-on-demand loop with live resize/move redraw.
What remains is optional polish — a thicker / anti-aliased string, a tunable feel (gravity /
damping / mouse-velocity "flick"), colour or glow, multiple strings — and eventually splitting the
renderer logic into its own library once a second consumer exists. See [TODO.md](../TODO.md) for the
running list.

---

*See [CONTRIBUTING.md](../CONTRIBUTING.md) for coding style and workflow, and
[DEV_ENV_SETUP.md](DEV_ENV_SETUP.md) for building from source.*
