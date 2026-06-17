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

#include "renderer.hpp"
#include <log/logger.hpp>
#include <signal/signal.hpp>
#include <window/window.hpp>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace
{

    //! Keep rendering this long after the last input so the (damped) string settles to rest
    //! before the render thread goes idle.
    constexpr float SETTLE_SECONDS = 6.0f;

    //! Cross-thread render command. The main thread (window events) emits these; the render
    //! thread consumes them.
    struct RenderEvent {
        enum class Type {
            Render, //!< Redraw (expose / move).
            Resize, //!< Window resized — width/height carry the new size.
            MouseMove, //!< Cursor moved — x/y carry the position (client pixels).
            Stop //!< Shut the render thread down.
        };

        Type type{Type::Render};
        uint32_t width{0};
        uint32_t height{0};
        int32_t x{0};
        int32_t y{0};
    };

    //! Context for the immediate window event callback (runs on the main/UI thread, including
    //! during Win32 modal resize/move loops). Forwards events to the render thread.
    struct CallbackContext {
        SignalsLib::Signal<RenderEvent>* signal;
        std::mutex* mutex;
        std::condition_variable* cv;
    };

    //! The render thread: owns the frame loop. Renders while the string is in motion (a settle
    //! window after the last event), otherwise sleeps on the condition variable. Because it is
    //! woken by the immediate event callback — which fires even during Win32 modal resize/move
    //! loops — the window keeps redrawing live, yet costs nothing when idle and settled.
    void renderThread(Engine::Renderer& renderer, uint32_t init_width, uint32_t init_height, SignalsLib::Signal<RenderEvent>& signal, std::mutex& mutex,
        std::condition_variable& cv)
    {
        using Clock = std::chrono::steady_clock;
        const Clock::duration settle = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(SETTLE_SECONDS));

        uint32_t width = init_width;
        uint32_t height = init_height;
        int32_t cursor_x = static_cast<int32_t>(init_width / 2);
        int32_t cursor_y = static_cast<int32_t>(init_height / 2);

        Clock::time_point last_time = Clock::now();
        Clock::time_point active_until = last_time + settle; // render the initial settle from gravity
        bool running = true;

        while (running) {
            // Block until the main thread signals an event when settled, or when the window is
            // minimised / zero-size (drawFrame would return immediately there, so the active loop
            // must not spin — treat zero-size as settled and sleep until a real resize wakes us).
            if ((Clock::now() >= active_until) || (width == 0) || (height == 0)) {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [&signal]() {
                    return !signal.empty();
                });
                last_time = Clock::now(); // avoid a huge dt after sleeping
            }

            // Drain all pending render events.
            bool got_input = false;
            RenderEvent ev;
            while (signal.consume(ev)) {
                switch (ev.type) {
                case RenderEvent::Type::Stop:
                    running = false;
                    break;
                case RenderEvent::Type::Resize:
                    width = ev.width;
                    height = ev.height;
                    got_input = true;
                    break;
                case RenderEvent::Type::MouseMove:
                    cursor_x = ev.x;
                    cursor_y = ev.y;
                    got_input = true;
                    break;
                case RenderEvent::Type::Render:
                    got_input = true;
                    break;
                }
            }
            if (!running) {
                break;
            }

            Clock::time_point now = Clock::now();
            if (got_input) {
                active_until = now + settle;
            }
            if ((now < active_until) && (width > 0) && (height > 0)) {
                float dt = std::chrono::duration<float>(now - last_time).count();
                last_time = now;
                renderer.drawFrame(width, height, cursor_x, cursor_y, dt);
            } else {
                last_time = now;
            }
        }
    }

} // namespace

//! Composition root — wires together the logger, the window and the Vulkan renderer.
int main()
{
    LoggingLib::Logger logger;
    logger.logInfo("StringWiggler starting.");

    WindowLib::WindowConfig config{};
    config.title = "StringWiggler";
    config.width = 800;
    config.height = 600;
    std::unique_ptr<WindowLib::Window> window{WindowLib::create(config, logger)};

    Engine::NativeWindowHandle window_handle{};
    window_handle.display = window->nativeDisplay();
    window_handle.window = window->nativeHandle();

    Engine::Renderer renderer;
    std::string error_message;
    if (!renderer.init(logger, window_handle, window->width(), window->height(), error_message)) {
        logger.logError(error_message);
        return EXIT_FAILURE;
    }

    // Render on a dedicated thread, woken by the window's immediate event callback. The callback
    // fires from the platform event handler even during Win32 modal resize/move loops, so the
    // window keeps redrawing live; the render thread sleeps once the string has settled.
    SignalsLib::Signal<RenderEvent> render_signal;
    std::mutex render_mutex;
    std::condition_variable render_cv;

    std::thread render_worker(renderThread, std::ref(renderer), window->width(), window->height(), std::ref(render_signal), std::ref(render_mutex), std::ref(render_cv));

    CallbackContext cb_ctx{&render_signal, &render_mutex, &render_cv};
    window->setEventCallback(
        [](const WindowLib::WindowEvent& ev, void* user_data) {
            auto* ctx = static_cast<CallbackContext*>(user_data);
            RenderEvent re{};
            switch (ev.type) {
            case WindowLib::WindowEvent::Type::Resize:
                re.type = RenderEvent::Type::Resize;
                re.width = ev.resize.width;
                re.height = ev.resize.height;
                break;
            case WindowLib::WindowEvent::Type::MouseMove:
                re.type = RenderEvent::Type::MouseMove;
                re.x = ev.mouse_move.x;
                re.y = ev.mouse_move.y;
                break;
            case WindowLib::WindowEvent::Type::Expose:
            case WindowLib::WindowEvent::Type::Move:
                re.type = RenderEvent::Type::Render;
                break;
            default:
                return; // Close is handled by the main loop; ignore keys/focus/etc.
            }
            // Emit under the mutex so the render thread cannot miss the wake-up (it checks the
            // queue under the same mutex inside cv.wait).
            {
                std::lock_guard<std::mutex> lock(*ctx->mutex);
                ctx->signal->emit(re);
            }
            ctx->cv->notify_one();
        },
        &cb_ctx);

    // Main loop — pump window events (blocking when idle). All rendering happens on the render
    // thread via the callback above; here we only watch for the close request.
    while (!window->shouldClose()) {
        window->waitEvents();

        WindowLib::WindowEvent event;
        while (window->pollEvent(event)) {
            if (event.type == WindowLib::WindowEvent::Type::Close) {
                window->requestClose();
            }
        }
    }

    // Stop the render thread and wait for it (renderer was created here, on the main thread, so
    // it is torn down here too — after the render thread has stopped using it).
    {
        std::lock_guard<std::mutex> lock(render_mutex);
        RenderEvent stop{};
        stop.type = RenderEvent::Type::Stop;
        render_signal.emit(stop);
    }
    render_cv.notify_one();
    render_worker.join();

    // Clear the callback before cb_ctx / the signal go out of scope, so a late event during
    // window destruction cannot invoke a dangling pointer.
    window->setEventCallback(nullptr, nullptr);

    renderer.destroy();
    logger.logInfo("StringWiggler shutting down.");
    return EXIT_SUCCESS;
}
