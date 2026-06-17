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

#pragma once

#include "device.hpp"
#ifdef _WIN32
#include <Volk/volk.h>
#else
#include <volk/volk.h>
#endif
#include <vulkan/vulkan_raii.hpp>
#include <string>

namespace Engine
{

    //! Graphics pipeline for the string: a single Vec2 vertex attribute, line-strip topology,
    //! dynamic rendering (no render pass), dynamic viewport/scissor. Loads line.spv (compiled
    //! from line.slang) sitting next to the executable.
    class Pipeline {
    public:
        Pipeline() = default;
        ~Pipeline() = default;

        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        Pipeline(Pipeline&&) = delete;
        Pipeline& operator=(Pipeline&&) = delete;

        //! Builds the pipeline for the given device and swapchain colour format. Returns false
        //! and fills out_error_message on failure (file + vk::raii errors caught here).
        [[nodiscard]] bool init(const Device& device, vk::Format colour_format, std::string& out_error_message);

        //! Releases the pipeline + layout. Safe to call repeatedly.
        void destroy();

        [[nodiscard]] const vk::raii::Pipeline& get() const
        {
            return m_pipeline;
        }

    private:
        vk::raii::PipelineLayout m_layout{nullptr}; //!< Empty layout (no descriptors / push constants yet).
        vk::raii::Pipeline m_pipeline{nullptr}; //!< The graphics pipeline.
    };

} // namespace Engine
