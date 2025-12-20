# StringWiggler - Development Plan

## Vision

A cross-platform simulator of a wiggling string. User moves the mouse cursor around the screen and a string (smooth polygon line) hangs from the cursor tip and whips around. A simple, satisfying, pointless-in-the-best-way app.

## Architecture

Three decoupled components wired via C-style callbacks in main():

```
                         ┌────────┐
                         │ Logger │
                         └────────┘
                           ▲    ▲
                  onLog   /      \   onLog
                         /        \
                ┌────────┐        ┌────────┐
                │ Window │◄──────►│ Engine │
                └────────┘ onDraw └────────┘
                     │    onResize     ▲
                     │                 │
                     │  native handle  │
                     └────────┬────────┘
                              │
                       ┌──────────┐
                       │ main.cpp │
                       │ (wiring) │
                       └──────────┘
```

- **Logger** — receives strings, writes to file. Knows nothing about Window or Engine.
- **Window** — creates native window, pumps events, fires callbacks. Knows nothing about Vulkan.
- **Engine** — Vulkan rendering. Receives native handle, renders frames. Knows nothing about windowing.
- **main.cpp** — composition root, wires everything together via callbacks.

### Callback Style

C-style function pointers with explicit user_data:

```cpp
void onLog(const char* message, void* user_data) {
    auto* logger = static_cast<Logger*>(user_data);
    logger->write(message);
}

window.setOnLog(onLog, &logger);
engine.setOnLog(onLog, &logger);
```

No std::function, no lambdas, no inheritance. Simple, debuggable, explicit.

### AppContext

Simple struct in main.cpp for callbacks that need multiple things:

```cpp
struct AppContext {
    Engine engine;
    Logger logger;
    // later: std::vector<Point2D> line_points;
};
```

## Directory Structure

```
/logger
    logger.h
    logger.cpp

/engine
    engine.h
    engine.cpp

/window
    window.h
    window_win32.cpp
    window_linux_x11.cpp
    window_linux_wayland.cpp
    window_macos.mm

main_win32.cpp
main_linux_x11.cpp
main_linux_wayland.cpp
main_macos.mm
CMakeLists.txt
```

## Component Interfaces

### Logger

```cpp
class Logger {
public:
    bool start(const char* filename);
    void write(const char* message);
    void stop();
};
```

### Window

```cpp
class Window {
public:
    void setOnLog(void(*callback)(const char*, void*), void* user_data);
    void setOnDraw(void(*callback)(void*), void* user_data);
    void setOnResize(void(*callback)(uint32_t, uint32_t, void*), void* user_data);
    void setOnClose(void(*callback)(void*), void* user_data);
    
    NativeWindowHandle getNativeHandle() const;
    void run();
};
```

### Engine (minimal first pass)

```cpp
class Engine {
public:
    void setOnLog(void(*callback)(const char*, void*), void* user_data);
    bool init(const NativeWindowHandle& window_handle);
    void destroy();
    
private:
    VkInstance m_vk_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_vk_debug_messenger = VK_NULL_HANDLE; // debug only
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
};
```

### Engine (full, later)

Internal structure:

- **VulkanContext** — VkInstance, VkDebugUtilsMessengerEXT
- **Device** — VkPhysicalDevice, VkDevice, VkQueue handles, queue family indices
- **Swapchain** — VkSurfaceKHR, VkSwapchainKHR, images, views, format, extent
- **Pipeline** — VkRenderPass, VkPipelineLayout, VkPipeline, framebuffers
- **FrameSync** — command pool, command buffers, fences, semaphores

## Implementation Order

### Phase 1: Refactor to new architecture

- [ ] Create `/logger`, `/engine`, `/window` directories
- [ ] Refactor Logger class (simplest, no dependencies)
- [ ] Refactor Engine class (minimal: VkInstance, device selection only)
- [ ] Create Window class (extract from current main_*.cpp files)
- [ ] Update main_*.cpp files to use new wiring pattern
- [ ] Update CMakeLists.txt

### Phase 2: Static line rendering

- [ ] Add Swapchain to Engine
- [ ] Add Pipeline (render pass, shaders, framebuffers)
- [ ] Add FrameSync (command buffers, synchronization)
- [ ] Add vertex buffer for line points
- [ ] Render static curved red line on black background

### Phase 3: Mouse tracking

- [ ] Add onMouseMove callback to Window
- [ ] Pass mouse position through to main
- [ ] Pin first line point to cursor position

### Phase 4: Physics

- [ ] Verlet integration for string nodes
- [ ] Gravity
- [ ] Distance constraints between nodes
- [ ] The string wiggles!

## Future

- Split into three independent libraries
- Engine becomes general purpose physics visualization
