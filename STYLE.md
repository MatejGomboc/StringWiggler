# Style Guide

Code style conventions for StringWiggler.

---

## General Rules

| Rule | Setting |
|------|---------|
| Indentation | 4 spaces (no tabs) |
| Max line length | 170 characters |
| Charset | UTF-8 |
| Final newline | Always |
| Trailing whitespace | Trim (except Markdown) |

These rules are enforced by `.editorconfig`. Install the EditorConfig plugin for your editor:

- **VS Code:** [EditorConfig for VS Code](https://marketplace.visualstudio.com/items?itemName=EditorConfig.EditorConfig)

---

## Single Source of Truth

Avoid duplicating information across files. Each piece of information should have one canonical location. Reference the canonical
source instead of copying its content, using `filename` § Section Name format.

| Information | Canonical Source |
|-------------|-----------------|
| Build & test commands | `README.md` § Building |
| Architecture & module layout | `docs/ARCHITECTURE.md` |
| Roadmap & what is not done yet | `TODO.md` |
| Formatting rules | `.clang-format`, `.editorconfig` |

When updating information, update the canonical source first.

---

## C++

The language standard is C++20 (the codebase uses `std::jthread` and `std::stop_token`).

### Formatting

Use `.clang-format` (LLVM-based, Allman braces for functions/namespaces, attached for classes/structs/enums). Format every `.cpp` and `.hpp` file before committing:

```bash
# Format all changed C++ files
clang-format -i src/*.cpp src/*.hpp

# Or a specific set of files
clang-format -i src/renderer.cpp src/device.cpp src/device.hpp
```

Key settings:

| Setting | Value |
|---------|-------|
| Brace style | Allman for functions/namespaces, attached for classes/structs/enums |
| Indent | 4 spaces |
| Column limit | 170 |
| Pointer alignment | Left (`int* ptr`) |
| Braces on bodies | Always — even single-line `if`/`else`/`for`/`while` (`InsertBraces: true`) |

### Naming Conventions

| Item | Convention | Example |
|------|------------|---------|
| Library namespaces | PascalCase + `Lib` suffix | `SignalsLib`, `LoggingLib`, `WindowLib`, `MathLib`, `TestingLib` |
| Application namespace | PascalCase | `Engine` |
| Types / Classes / Structs | PascalCase | `Window`, `WindowConfig`, `Vec2` |
| Functions / Methods | camelCase | `createSurface`, `pollEvent`, `logInfo` |
| Constants | SCREAMING_SNAKE_CASE | `MAX_FRAMES_IN_FLIGHT` |
| Local variables | snake_case | `frame_index` |
| Member variables | m_snake_case | `m_device` |
| Macros | SCREAMING_SNAKE_CASE | `VK_NO_PROTOTYPES` |

### Error Handling — Our Code Does Not Throw

Our own code never throws and never lets an exception cross a public interface. Functions that can fail
return `bool` and write a human-readable message into a `std::string& out_error_message` out-parameter:

```cpp
//! Creates the Vulkan surface for the given window. Returns false and fills out_error_message on failure.
[[nodiscard]] bool createSurface(const vk::raii::Instance& instance, const NativeWindowHandle& handle, vk::raii::SurfaceKHR& out_surface, std::string& out_error_message);
```

The caller checks the result and routes the message to the logger:

```cpp
std::string error;
if (!createSurface(m_instance.get(), handle, m_surface, error))
{
    m_logger.logError("Failed to create surface: " + error);
    return false;
}
```

The external Vulkan library is the one place exceptions exist: `vk::raii` / vulkan-hpp throw a
`vk::SystemError` on Vulkan errors. We let them throw — but we **catch them at the component `init()`
boundary** and translate to `out_error_message`, so no exception ever escapes our API:

```cpp
try {
    m_instance = vk::raii::Instance(m_context, create_info);
} catch (const vk::SystemError& e) {
    out_error_message = std::string("Vulkan error: ") + e.what();
    return false;
}
```

The other place exceptions are allowed is test code: the `TestingLib` framework signals failures by
throwing (`TEST_CHECK`, `TEST_CHECK_EQUAL`, `TEST_CHECK_THROWS`), so test targets are built with
exceptions enabled. Never throw your own exceptions from a library or from `src/`.

### Type Explicitness

Do not use `auto` — write the explicit type so the reader never has to guess. The only exception
is where the type is impossible to spell (lambdas).

```cpp
// Correct
uint32_t count = static_cast<uint32_t>(devices.size());
VkPhysicalDeviceProperties props{};

// Wrong
auto count = static_cast<uint32_t>(devices.size());

// Exception — lambdas have unspellable types
auto on_event = [&](const WindowLib::WindowEvent& event) { ... };
```

### Cross-Component Decoupling

For callbacks across component boundaries, use a C-style function pointer plus a `void* user_data`
context pointer — **not** `std::function`. This keeps interfaces ABI-thin and free of hidden
allocations. `WindowLib`'s `EventCallback` is the reference example.

```cpp
using EventCallback = void (*)(const WindowEvent& event, void* user_data);
```

### Attributes

Use `[[nodiscard]]` on every function that returns a value the caller must not silently discard —
getters, factory functions, query functions, and any `bool`-returning function whose result reports
success or failure.

```cpp
[[nodiscard]] VkDevice get() const;
[[nodiscard]] uint32_t graphicsFamilyIndex() const;
[[nodiscard]] bool createSurface(...);
```

### Operator Precedence

Use explicit parentheses when combining arithmetic, bitwise, or increment/decrement operators
with comparison or logical operators. Do not rely on the reader knowing precedence rules:

```cpp
// Correct — each sub-expression is explicit
if ((++frame_counter) % 60 == 0) { ... }
if ((flags & VK_QUEUE_GRAPHICS_BIT) != 0) { ... }

// Wrong — relies on implicit precedence
if (++frame_counter % 60 == 0) { ... }
if (flags & VK_QUEUE_GRAPHICS_BIT != 0) { ... }
```

In compound conditions, parenthesise each sub-expression so it is visually clear how the operations
belong together:

```cpp
// Correct
if ((width == 0) || (height == 0)) { ... }

// Wrong
if (width == 0 || height == 0) { ... }
```

A single boolean variable does not need extra parentheses — the intent is already obvious:

```cpp
// Fine
if (m_dying) { ... }
```

### Constants

Use `constexpr` for compile-time constants, named in `SCREAMING_SNAKE_CASE`. Do not use plain
`const` or magic numbers where `constexpr` applies.

```cpp
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
constexpr float QUEUE_PRIORITY = 1.0f;
```

### Include Order

All `#include` directives are flush — no blank lines between groups. Order:

1. Same-module headers (`"device.hpp"`)
2. Project library headers (`<log/logger.hpp>`, `<window/window.hpp>`)
3. Volk / Vulkan (`<volk.h>`)
4. Standard library headers (`<vector>`, `<string>`)

```cpp
#include "device.hpp"
#include "instance.hpp"
#include <log/logger.hpp>
#include <window/window.hpp>
#include <volk.h>
#include <cstdint>
#include <string>
#include <vector>
```

### Comment Alignment

Do not column-align trailing comments. Use a single space before `//` or `//!<`:

```cpp
// Correct
SignalsLib::Signal<LogMessage> m_queue; //!< Thread-safe message queue.
std::mutex m_mutex; //!< Protects the wake-up condition.

// Wrong — padded to align
SignalsLib::Signal<LogMessage> m_queue;  //!< Thread-safe message queue.
std::mutex m_mutex;                       //!< Protects the wake-up condition.
```

The same applies to enum values — no extra spaces between the value and its comment.

### Constructor Initialiser Lists

Always break after the colon. Each initialiser gets its own line with 4-space indentation. A single
initialiser is one line; multiple initialisers are one per line:

```cpp
Win32Window::Win32Window(const WindowConfig& config, LoggingLib::Logger& logger) :
    Window(logger)
{
}

Device::Device(VkInstance instance, VkSurfaceKHR surface, LoggingLib::Logger& logger) :
    m_logger(logger),
    m_instance(instance),
    m_surface(surface)
{
}
```

### Member Initialisation

Use brace initialisation `{}` for all member default values — not `= value` assignment:

```cpp
// Correct
uint32_t m_width{0};
bool m_dying{false};
VkDevice m_device{VK_NULL_HANDLE};

// Wrong
uint32_t m_width = 0;
bool m_dying = false;
```

### Vulkan: vulkan-hpp + vk::raii + VMA

StringWiggler uses **vulkan-hpp with `vk::raii`** for Vulkan handles and **VMA** (Vulkan Memory
Allocator) for GPU memory. `vk::raii` gives automatic, ordered teardown; its exceptions are contained at
the `init()` boundary (see the error-handling section above).

- Vulkan is loaded dynamically: `VK_NO_PROTOTYPES` is defined, `volk.cpp` provides `VOLK_IMPLEMENTATION`,
  and `volkInitialize()` / `volkLoadInstanceOnly()` / `volkLoadDevice()` set up the entry points. The
  vulkan-hpp dynamic dispatcher (`VULKAN_HPP_DISPATCH_LOADER_DYNAMIC`) is fed Volk's
  `vkGetInstanceProcAddr` and re-`init`-ed after the instance and device exist.
- Handles are `vk::raii::` types (`vk::raii::Instance`, `vk::raii::Device`, `vk::raii::SurfaceKHR`, …).
  Declare them in dependency order so reverse-order member destruction tears them down correctly; reset
  one early by assigning `nullptr`.
- GPU memory goes through VMA: the `Allocator` plus the move-only `AllocatedBuffer` / `AllocatedImage`
  RAII wrappers, fed Volk's function pointers via `vmaImportVulkanFunctionsFromVolk`.
- Prefer the vulkan-hpp value types and setters; raw C structs are fine for the VMA create-info (VMA is
  a C API).

```cpp
vk::InstanceCreateInfo create_info{};
create_info.setPApplicationInfo(&app_info);
create_info.setPEnabledExtensionNames(extensions);
m_instance = vk::raii::Instance(m_context, create_info);
```

### Resource Ownership

RAII everywhere. `vk::raii::` handles destroy themselves in reverse member-declaration order, so order
your members by dependency (instance, surface, device, allocator). Where explicit ordered teardown is
wanted, an owner exposes an idempotent `destroy()` that resets its handles in reverse order — assigning
`nullptr` to a `vk::raii` handle destroys it. Use `std::unique_ptr` for single-owner heap objects (the
`WindowLib::create()` factory returns `std::unique_ptr<Window>`). Never use raw `new` / `delete` for
ownership.

```cpp
void Device::destroy()
{
    // Assigning nullptr to a vk::raii handle destroys it; logical device last.
    m_present_queue = nullptr;
    m_graphics_queue = nullptr;
    m_device = nullptr;
    m_physical_device = nullptr;
}
```

See `docs/ARCHITECTURE.md` for the full ownership hierarchy (the `Renderer` is the composition root that
owns the `Instance`, surface, and `Device`).

### Doxygen Comments

All doxygen comments are proper sentences — capital letter start, full stop end.

| Style | Use | Example |
|-------|-----|---------|
| `//!` | Single-line brief | `//! Returns the current extent.` |
| `//!<` | Inline member | `uint32_t m_width{0}; //!< Client-area width in pixels.` |
| `/*! */` | Multi-line block | See below |

Multi-line doxygen blocks use 4-space indented content and Qt-style backslash commands (`\param`,
`\return`, `\brief`) — not Javadoc `@` prefixes:

```cpp
/*!
    Selects the best physical device for rendering, preferring discrete GPUs.

    \param instance The Vulkan instance to enumerate devices from.
    \param surface The target surface used to check present support.
    \param out_error_message Receives a description when selection fails.
    \return true on success, false otherwise.
*/
```

### Licence Header

Every source file begins with the GPLv3 header block in a plain `/* */` comment (not doxygen):

```cpp
/*
    Copyright (C) 2025 Matej Gomboc https://github.com/MatejGomboc/StringWiggler

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
*/
```

"GNU General Public License" is a proper noun and keeps its American spelling; British "licence" is used
only in ordinary prose.

---

## YAML (GitHub Actions)

- **4-space** indentation for structure levels.
- List items use 2-space continuation from the `-` character (standard YAML).
- Shell content inside `run: |` blocks uses 4-space indentation for shell constructs.
- Blank line between top-level keys and between jobs; comments on their own line, not inline.

```yaml
jobs:
    build:
        runs-on: ubuntu-latest

        steps:
            - name: Checkout
              uses: actions/checkout@v4

            - name: Build
              run: cmake --workflow --preset=linux-gcc
```

---

## JSON

**4 spaces** indentation.

```json
{
    "key": "value",
    "nested": {
        "item": 123
    }
}
```

---

## Markdown

- ATX-style headings with blank lines before and after.
- `-` for unordered lists, `1.` for ordered lists.
- Always specify a language on fenced code blocks.
- Markdown files are exempt from trailing-whitespace trimming (needed for line breaks).

---

## British Spelling

Use British spelling in all documentation, comments, and user-facing strings:

| American | British |
|----------|---------|
| color | colour |
| optimize | optimise |
| behavior | behaviour |
| center | centre |
| license | licence |
| synchronize | synchronise |
| initialize | initialise |
| recognize | recognise |

Code identifiers may use American spelling where it matches library/API conventions (for example, Vulkan
API names such as `VkColorSpaceKHR`).

---

## Code Quality Tooling

### Compiler Warnings

Warnings are errors on all compilers — zero-warning policy:

- **MSVC:** `/EHsc /W4 /WX`
- **GCC/Clang:** `-Wall -Wextra -Wpedantic -Werror`

### Static Analysis (Clang-Tidy)

Configuration is in `.clang-tidy`. It runs via `clangd` in editors such as VS Code. To suppress a
specific check on a line:

```cpp
int x = legacy_function(); // NOLINT(bugprone-unused-return-value)
```

### Runtime Sanitisers

CMake presets for sanitiser builds (Linux Clang only):

```bash
# AddressSanitizer + UndefinedBehaviorSanitizer
cmake --workflow --preset=linux-clang-asan

# ThreadSanitizer (cannot be combined with ASan)
cmake --workflow --preset=linux-clang-tsan
```

Known-benign TSan reports are listed in `tsan_suppressions.txt`.

### Vulkan Validation (Debug Builds)

Debug builds create a debug messenger and route the Vulkan validation layers through
`LoggingLib::Logger`, so validation output appears alongside the application's own log messages.

---

*Last updated: 2026-06-17*
