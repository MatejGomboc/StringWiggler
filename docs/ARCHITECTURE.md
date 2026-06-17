# Architecture

Technical architecture of StringWiggler.

> StringWiggler is a deliberately-pointless Vulkan toy: a string dangles from the mouse cursor
> and whips around as you move it. This document describes the real structure of the codebase.
> Parts that are not built yet are marked **(target design)**. See [TODO.md](../TODO.md) for the
> development journal.

---

## Overview

StringWiggler is a tiny cross-platform application written in C++20 on top of Vulkan. It is split
into a handful of small, self-contained libraries under `libs/` and a thin application under `src/`
(the `Engine` namespace). The libraries are general-purpose building blocks; the application wires
them together and owns the Vulkan back end.

At the present early stage the application opens a window, initialises Vulkan (instance, surface,
device selection) and then idles. Nothing is drawn yet — mouse tracking, the string itself, the
physics and the rendering are all still to come.

External dependencies are minimal: the Vulkan SDK and volk (the dynamic Vulkan loader). Vulkan is
used through plain C with `VK_NO_PROTOTYPES` and `VK_NULL_HANDLE`-style handles; there is no
vulkan-hpp, no `vk::raii`, and no memory allocator library. Vulkan 1.3 or newer is required.

```text
┌──────────────────────────────────────────────────────────────┐
│                    Application  (src/, Engine)                 │
│  main.cpp — composition root, event loop                      │
│  Renderer ── Instance · surface · Device                      │
├──────────────────────────────────────────────────────────────┤
│                       Libraries  (libs/)                       │
│   window  (Win32 / XCB)        math  (Vec2/Vec3/Vec4)         │
│      │                                                         │
│   logging  (async console logger)                             │
│      │                                                         │
│   signals  (Signal<T> FIFO queue)                             │
│                                                                │
│   testing  (unit-test framework — test targets only)         │
├──────────────────────────────────────────────────────────────┤
│                     volk  (dynamic loader)                     │
│  Resolves Vulkan function pointers at runtime                 │
└──────────────────────────────────────────────────────────────┘
```

---

## Layering and Dependencies

The libraries form a small dependency stack. Nothing reaches upward; each layer only knows about
the ones below it.

```text
        app (src/, Engine)
        │      │      │
        ▼      ▼      ▼
     window  logging  math
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
- **`libs/math`** — INTERFACE, namespace `MathLib`. `Vec2` / `Vec3` / `Vec4`. The 2D string physics
  will use `Vec2`. Header: `<math/vector.hpp>`. No dependencies.
- **`libs/window`** — STATIC, namespace `WindowLib`. The window abstraction and platform backends.
  Depends on `logging`. Header: `<window/window.hpp>`.
- **`libs/testing`** — STATIC, namespace `TestingLib`. An in-house unit-test framework (~250 lines):
  `TEST_CASE` auto-registration, `TEST_CHECK` / `TEST_CHECK_EQUAL` / `TEST_CHECK_THROWS`, and
  `runAll()`. Header: `<testing/testing.hpp>`. Linked only by test targets, never by the application.
- **`src/` (the application)**, namespace `Engine`. Depends on `window`, `logging` and `math`. Owns
  the Vulkan back end through `Renderer`.

Library namespaces are PascalCase with a `Lib` suffix; the application uses `Engine`. Each library
has its own `include/<name>/` directory, its own `tests/` directory linking `testing`, and a plain
CMake target name (`signals`, `logging`, `math`, `window`, `testing`).

---

## Key Design Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| Vulkan loading | volk (dynamic) | No static link; `VK_NO_PROTOTYPES` defined globally |
| Vulkan bindings | Plain C + `VK_NULL_HANDLE` handles | Small toy; no vulkan-hpp / `vk::raii` to learn |
| Vulkan version | 1.3+ required | Modern baseline; dynamic rendering available later |
| Error handling | Return `bool` + `std::string& out_error_message` | No exceptions in production code |
| Cross-component callbacks | C function pointers + `void* user_data` | Simple, debuggable, no `std::function` |
| Resource ownership | RAII; explicit `destroy()` in reverse order for Vulkan handles | Predictable teardown |
| Inter-thread messaging | `SignalsLib::Signal<T>` | One thread-safe queue type, reused by the logger |
| Logging | Async, severity-based, to console | Console-subsystem app; Debug/Info → stdout, the rest → stderr |
| Window backends | Win32 and XCB only | Matches the platforms we actually support |
| Native handles | Exposed as `void*` | Consumers never include platform headers |
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

### Vulkan back end (`src/`, `Engine`) *(built: instance/surface/device)*

`Engine::Renderer` is the composition root for Vulkan. It owns, in dependency order:

```text
Renderer
  ├── Instance     (VkInstance + debug messenger)
  ├── surface      (VkSurfaceKHR — owned directly by Renderer)
  └── Device       (physical + logical device, graphics & present queues)
```

- **`Engine::Instance`** initialises volk, creates the `VkInstance` and, in debug builds, a
  `VkDebugUtilsMessengerEXT` whose validation output is routed to the `Logger`.
- **`surface.hpp`** provides two free functions: `requiredSurfaceExtensions()` (the WSI extensions
  for the current platform) and `createSurface()` (builds a `VkSurfaceKHR` from a
  `NativeWindowHandle`). Keeping both together concentrates all platform WSI knowledge in one file.
- **`Engine::Device`** scores the available physical devices, preferring discrete GPUs, and requires
  graphics + present queue families and the `VK_KHR_swapchain` extension before creating the logical
  device and fetching the queues.

`main.cpp` is the single entry point (`int main()`, console subsystem). It constructs the `Logger`,
creates the `Window`, builds a `NativeWindowHandle` from the window's `void*` handles, initialises
the `Renderer`, then runs the event loop (`waitEvents()` → drain `pollEvent()` → handle close).

---

## Error Handling

Production code does not use exceptions. Operations that can fail return `bool` and write a
human-readable reason into a `std::string& out_error_message` out-parameter, for example:

```cpp
bool Renderer::init(LoggingLib::Logger& logger, const NativeWindowHandle& window_handle, std::string& out_error_message);
```

The caller logs the message and exits. Test code is the one exception to the no-exceptions rule:
`TestingLib` assertions throw, so test targets are built with exceptions enabled.

Resource ownership is RAII. Vulkan handles are released by an explicit `destroy()` on each owner,
called in reverse construction order (Device → surface → Instance), and each `destroy()` is safe to
call more than once.

---

## Threading

The application itself is single-threaded: the window pumps native events and the (future) frame
loop both run on the main thread. The one background thread in the codebase belongs to the
**logger**: its `std::jthread` worker drains the message queue and performs the console writes off
the calling thread. Because of this, shutdown is not instantaneous — the log queue must drain and
the worker must join before the process exits.

`SignalsLib::Signal<T>` guards its queue with a mutex, so producers on any thread and the single
logger consumer interleave safely.

---

## Shutdown

Teardown is ordered and idempotent:

- The `Renderer` calls `destroy()` on its members in reverse construction order. Each Vulkan owner
  guards against a second call so a manual `destroy()` followed by the destructor is harmless.
- The `Logger` is destroyed last in `main`. Its `std::jthread` requests stop via the `std::stop_token`,
  the worker drains any remaining messages, and the destructor joins the thread.

There is no elaborate multi-phase shutdown protocol — RAII plus reverse-order `destroy()` is enough
for a toy of this size.

---

## Future Phases *(target design)*

The drawing pipeline is not built yet. The intended order is:

1. **Swapchain** — add `VK_KHR_swapchain` creation and image views to the renderer.
2. **Pipeline** — a minimal graphics pipeline and the per-frame command-buffer / synchronisation
   plumbing (acquire → record → submit → present), with swapchain recreation driven by
   `VK_ERROR_OUT_OF_DATE_KHR` / `VK_SUBOPTIMAL_KHR`.
3. **Static line** — upload a fixed set of `MathLib::Vec2` points and draw a static line on a
   plain background.
4. **Mouse tracking** — feed `WindowEvent::MouseMove` through to the renderer and pin the first
   point of the line to the cursor.
5. **Verlet physics** — integrate the string nodes with Verlet integration, gravity and distance
   constraints between nodes, so the string finally wiggles.

Further out, the three libraries (window, logging, and the renderer logic) may be split into fully
independent libraries; see [TODO.md](../TODO.md).

---

*See [CONTRIBUTING.md](../CONTRIBUTING.md) for coding style and workflow, and
[DEV_ENV_SETUP.md](DEV_ENV_SETUP.md) for building from source.*
