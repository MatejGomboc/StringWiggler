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
- **`src/`** — the application (namespace `Engine`): the renderer has been
  decomposed into `Instance` (VkInstance + debug messenger, validation routed to
  the logger in debug builds), `surface` (free functions), `Device` (scored
  physical-device selection preferring discrete GPUs) and `Renderer` (the
  composition root owning Instance + surface + Device). A single console
  `main.cpp` wires it all together.
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

The app currently opens a window and initialises Vulkan (instance, surface,
device selection), then idles. Nothing is drawn yet. The remaining work:

| Phase | Goal | Status |
| --- | --- | --- |
| Phase 2 | Swapchain + graphics pipeline + static line rendering — draw a curved line on a cleared background. | To do |
| Phase 3 | Mouse tracking — pin the string head to the cursor. The `WindowEvent` `MouseMove` already carries `x`/`y` plus `dx`/`dy`. | To do |
| Phase 4 | Verlet physics — gravity + distance constraints over the string nodes using `MathLib::Vec2`, so the string actually wiggles. | To do |

---

## Later

- Profile and tidy the physics step once the string is moving.
- Tighten the renderer's resize/recreation handling once a swapchain exists.
