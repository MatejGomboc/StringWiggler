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
#include <window/window.hpp>
#include <cstdlib>
#include <memory>
#include <string>

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
    if (!renderer.init(logger, window_handle, error_message)) {
        logger.logError(error_message);
        return EXIT_FAILURE;
    }

    // Event loop — block until events arrive, drain them, react to close requests.
    // Frame rendering will hang off this loop in a later milestone.
    while (!window->shouldClose()) {
        window->waitEvents();

        WindowLib::WindowEvent event;
        while (window->pollEvent(event)) {
            if (event.type == WindowLib::WindowEvent::Type::Close) {
                window->requestClose();
            }
        }

        // TODO: render a frame here once the swapchain/pipeline exist (Phase 2).
    }

    renderer.destroy();
    logger.logInfo("StringWiggler shutting down.");
    return EXIT_SUCCESS;
}
