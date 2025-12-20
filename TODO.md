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
                     │    onClosing    ▲
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

### Initialization Order

Components must be initialized in this order:

1. **Logger** — so others can log during their init
2. **Window** — creates native window, provides handle
3. **Engine** — receives handle from Window to create Vulkan surface

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

### Threading Model

Single instance of each component, but internal threads exist:

- **Window** — runs on main thread, pumps native event loop
- **Engine** — called from main thread, but internally waits on GPU (`vkDeviceWaitIdle`)
- **Logger** — has internal writer thread doing async file I/O (main thread queues messages, writer thread flushes to disk)

This means shutdown can take time — GPU must finish, log queue must drain.

### Error Handling

Fail fast, die gracefully. Every runtime error is treated as fatal:

```cpp
void Engine::render(...) {
    if (m_dying) return;
    
    VkResult result = vkSomething(...);
    if (result != VK_SUCCESS) {
        // Error = immediate self-destruction
        destroy();  // triggers onIsAboutToStop → cleanup → onHasStopped
        return;
    }
}
```

No partial recovery states. No error codes bubbling up. Component encounters error → component initiates its own death → others get notified via `onIsAboutToStop` / `onHasStopped` and can react accordingly.

This keeps the code simple — no complex error recovery paths, no inconsistent states to manage.

### Resize Handling

Deferred and Vulkan-driven — let Vulkan tell us when swapchain is stale:

```cpp
void Engine::render(...) {
    if (m_dying) return;
    
    VkResult result = vkAcquireNextImageKHR(...);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();  // query current window size internally
        return;  // skip this frame, try next
    }
    
    // ... render ...
    
    result = vkQueuePresentKHR(...);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    }
}
```

**Benefits:**
- Simple — no onResize callback needed
- Robust — handles all cases (resize, minimize, display change)
- Lazy — only recreate when actually needed

**Minimized window (0x0):** Skip frame entirely — can't create swapchain with zero dimensions.

### Shutdown Pattern

Each component uses a `m_dying` flag and a two-phase notification protocol:

```cpp
void Engine::destroy() {
    // 1. Prevent re-entry (atomic for thread safety)
    if (m_dying.exchange(true)) return;
    
    // 2. "Stop sending me work!"
    if (m_on_is_about_to_stop) {
        m_on_is_about_to_stop(m_on_is_about_to_stop_user_data);
    }
    
    // 3. Disconnect most callbacks (they can no longer reach us)
    m_on_log = nullptr;
    m_on_is_about_to_stop = nullptr;
    
    // 4. Clean up resources (might take time - waiting on GPU)
    vkDeviceWaitIdle(m_device);
    vkDestroyDevice(m_device, nullptr);
    // ...
    
    // 5. "I'm truly dead now"
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
- `onIsAboutToStop` — "Stop sending me work!" (others stop calling)
- `onHasStopped` — "I'm truly dead now" (safe for others to proceed)

**Example flows:**

Engine shutdown:
```
engine.destroy() called
    │
    ├─► onIsAboutToStop ──► "Stop sending me render calls!"
    │
    ├─► vkDeviceWaitIdle() ──► waits for GPU to finish
    │
    └─► onHasStopped ──► "GPU idle, Vulkan destroyed, I'm truly dead"
```

Logger shutdown:
```
logger.stop() called
    │
    ├─► onIsAboutToStop ──► "Stop sending me log messages!"
    │
    ├─► flush queue + join writer thread ──► waits for disk writes
    │
    └─► onHasStopped ──► "File closed, I'm truly dead"
```

**Thread Safety:**
- `m_dying` must be `std::atomic<bool>` — checked by internal threads
- Use `m_dying.exchange(true)` for atomic test-and-set in destroy()
- Logger's writer thread checks `m_dying` to know when to exit its loop
- Callbacks wired once at startup, never modified during runtime

**Guarantees:**
- **Re-entry blocked** — atomic flag prevents destroy() from running twice
- **Circular cascades broken** — any callback chain that circles back hits the flag
- **Late calls ignored** — methods silently return if component is dying
- **Destruction order does not matter** — each component protects itself independently
- **Handshake protocol** — dependents can wait for `onHasStopped` before proceeding

### AppContext

Simple struct in main.cpp for callbacks that need multiple things:

```cpp
struct Point2D {
    float x;
    float y;
};

struct AppContext {
    Logger logger;
    Window window;
    Engine engine;
    std::vector<Point2D> line_points;
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
    void write(const char* message);  // queues message, returns immediately
    void stop();

private:
    std::atomic<bool> m_dying = false;
    std::thread m_writer_thread;      // internal thread for async file I/O
};
```

### Window

```cpp
class Window {
public:
    void setOnLog(void(*callback)(const char*, void*), void* user_data);
    void setOnDraw(void(*callback)(void*), void* user_data);
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
