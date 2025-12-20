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
                │ Window │───────►│ Engine │
                └────────┘ onDraw └────────┘
                     │    onResize     ▲
                     │    onClosing    │
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

### Shutdown Pattern

Each component uses a `m_dying` flag and a two-phase notification protocol:

```cpp
void Engine::destroy() {
    // 1. Prevent re-entry (atomic for thread safety)
    if (m_dying.exchange(true)) return;
    
    // 2. "I'm about to stop" - others can prepare
    if (m_on_is_about_to_stop) {
        m_on_is_about_to_stop(m_on_is_about_to_stop_user_data);
    }
    
    // 3. Disconnect most callbacks (they can no longer reach us)
    m_on_log = nullptr;
    m_on_is_about_to_stop = nullptr;
    
    // 4. Clean up resources (might take time)
    vkDeviceWaitIdle(m_device);
    vkDestroyDevice(m_device, nullptr);
    // ...
    
    // 5. "I have stopped" - safe for others to proceed with their own destruction
    if (m_on_has_stopped) {
        m_on_has_stopped(m_on_has_stopped_user_data);
    }
    m_on_has_stopped = nullptr;
}
```

Other methods also check the flag:

```cpp
void Engine::render(...) {
    if (m_dying) return;  // Silently ignore calls to dying component
    // ...
}
```

**Two-phase notifications:**
- `onIsAboutToStop` — fired before cleanup, callbacks still connected, others can prepare
- `onHasStopped` — fired after cleanup complete, safe for dependents to proceed with their own destruction

**Example flow:**
```
Window.onClosing fires
    │
    ▼
main.cpp handler calls engine.destroy()
    │
    ├─► Engine fires onIsAboutToStop ──► Logger: "Engine shutting down..."
    │
    ├─► Engine does vkDeviceWaitIdle, destroys Vulkan resources
    │
    └─► Engine fires onHasStopped ──► Window: "Engine is dead, I can die now"
```

**Thread Safety:**
- `m_dying` must be `std::atomic<bool>` to handle concurrent access
- Use `m_dying.exchange(true)` for atomic test-and-set in destroy()
- Callback pointers should be protected if modified from multiple threads (mutex or atomic)

**Guarantees:**
- **Re-entry blocked** — atomic flag prevents destroy() from running twice
- **Circular cascades broken** — any callback chain that circles back hits the flag
- **Late calls ignored** — methods silently return if component is dying
- **Destruction order does not matter** — each component protects itself independently
- **Handshake protocol** — dependents can wait for `onHasStopped` before proceeding

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

private:
    std::atomic<bool> m_dying = false;
};
```

### Window

```cpp
class Window {
public:
    void setOnLog(void(*callback)(const char*, void*), void* user_data);
    void setOnDraw(void(*callback)(void*), void* user_data);
    void setOnResize(void(*callback)(uint32_t, uint32_t, void*), void* user_data);
    void setOnClosing(void(*callback)(void*), void* user_data);
    void setOnIsAboutToStop(void(*callback)(void*), void* user_data);
    void setOnHasStopped(void(*callback)(void*), void* user_data);
    
    NativeWindowHandle getNativeHandle() const;
    void run();
    void close();  // triggers destruction sequence

private:
    std::atomic<bool> m_dying = false;
};
```

### Engine (minimal first pass)

```cpp
class Engine {
public:
    void setOnLog(void(*callback)(const char*, void*), void* user_data);
    void setOnIsAboutToStop(void(*callback)(void*), void* user_data);
    void setOnHasStopped(void(*callback)(void*), void* user_data);
    bool init(const NativeWindowHandle& window_handle);
    void destroy();
    
private:
    std::atomic<bool> m_dying = false;
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
