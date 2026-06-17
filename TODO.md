# TODO & Development Plan

A short task list and journal for StringWiggler. The detailed design lives in
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — this file tracks what is built
and what is left.

**Vision:** a small, deliberately-pointless cross-platform Vulkan toy — a
string dangles from the mouse cursor and whips around as you move it.

---

## Done

The original aspirational architecture has now largely been built. The codebase
has been split into modular `libs/` plus a `src/` application, wired by CMake:

- **`libs/signals`** — INTERFACE (header-only) lib, namespace `SignalsLib`: a
  thread-safe typed FIFO queue `Signal<T>` (emit/consume).
- **`libs/testing`** — STATIC lib, namespace `TestingLib`: a small in-house
  unit-test framework (`TEST_CASE` auto-registration, `TEST_CHECK` /
  `TEST_CHECK_EQUAL` / `TEST_CHECK_THROWS`, `runAll()`).
- **`libs/logging`** — STATIC lib, namespace `LoggingLib`: the `Logger` is now
  async, thread-safe and severity-based (`Debug`/`Info`/`Warning`/`Error`/
  `Fatal`) with a `std::jthread` + `std::stop_token` RAII worker. It writes to
  the **console** (Debug/Info to stdout, Warning/Error/Fatal to stderr) — the
  old `log.txt` file sink is gone, since the app is now a console-subsystem
  programme. Depends on `signals`.
- **`libs/math`** — INTERFACE lib, namespace `MathLib`: `Vec2`/`Vec3`/`Vec4`
  (the 2D string physics will use `Vec2`).
- **`libs/window`** — STATIC lib, namespace `WindowLib`: an abstract `Window`
  base class + `WindowConfig` + a `create(config, logger)` factory returning
  `std::unique_ptr<Window>`, a platform-neutral `WindowEvent` tagged union, an
  internal event queue drained by `pollEvent()` plus an optional immediate
  `EventCallback`. Native handles are exposed generically as `void*` so
  consumers never include platform headers. Depends on `logging`.
- **`src/`** — the application (namespace `Engine`). The Vulkan back end is
  decomposed into `Instance` (VkInstance + debug messenger, validation routed to
  the logger in debug builds), `surface` (free functions), `Device` (scored
  selection preferring discrete GPUs, falling back to integrated; requires a
  graphics+compute+present queue and Vulkan 1.3 dynamic rendering +
  synchronization2), `Allocator` (VMA + RAII buffer/image wrappers), `Swapchain`
  (FIFO present, dynamic-rendering images), `Pipeline` (graphics line-strip) and
  `ComputePipeline` (the physics dispatch), all owned by the `Renderer`
  composition root. The string is simulated on the GPU and drawn each frame as a
  128-node line strip; `main.cpp` runs the window event loop on the main thread
  and the frame loop on a dedicated render thread.
- **Tooling** — unit tests via CTest (`enable_testing()`), CMake presets,
  warnings-as-errors on all compilers, ASan/UBSan + TSan sanitiser presets, and
  the supporting docs/CI have been adopted.

Two decisions worth recording:

- **Platform scope** — supported targets are **Windows (Win32)** and
  **Linux (X11 via XCB)**. macOS and Wayland were dropped in this refactor to
  match the Win32+XCB reach; Wayland users rely on XWayland.
- **Logger sink** — the logger now writes to **stdout/stderr** instead of a
  `log.txt` file.

---

## Roadmap

The original drawing + physics roadmap is now complete — the app renders a
GPU-simulated wiggling string that follows the cursor:

| Phase | Goal | Status |
| --- | --- | --- |
| Phase 2 | Swapchain + graphics pipeline + static line on a cleared background (dynamic rendering, Slang shaders). | Done |
| Phase 3 | Mouse tracking — pin the string head to the cursor (screen pixels → NDC). | Done |
| Phase 4 | Physics — Verlet integration + gravity + distance constraints over the nodes, run in a Slang **compute** shader (per-node, one workgroup), with velocity damping so it settles. | Done |
| Phase 5 | Live window events — dedicated render thread woken by the window `EventCallback`; render-on-demand (idle costs nothing); redraw live on resize/move; new `Move` event. | Done |

---

## Later

Polish and possible extensions (none essential — this is a toy):

- Thicker / anti-aliased string (expand the line into a triangle strip, or MSAA).
- Tunable feel — expose gravity / damping / segment count, or add mouse-velocity
  "flick" so fast moves whip the string harder.
- Visual flourishes — a colour gradient along the string, glow, a non-black clear.
- Multiple strings, or pin both ends.
- Split the renderer logic into its own library once a second consumer exists
  (the `libs/` split anticipated in the original design).
