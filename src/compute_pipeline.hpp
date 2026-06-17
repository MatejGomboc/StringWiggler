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
#include <cstdint>
#include <string>

namespace Engine
{

    //! Push constants for the physics compute shader. Must match the PhysicsPush struct in
    //! physics.slang (scalar/packed layout — all members are 4-byte aligned).
    struct PhysicsPush {
        float cursor_x; //!< Head target X (NDC).
        float cursor_y; //!< Head target Y (NDC).
        float dt; //!< Frame delta time (seconds, clamped).
        float gravity; //!< Downward acceleration (NDC / s^2; +Y is down).
        float segment_length; //!< Rest distance between adjacent nodes (NDC).
        float damping; //!< Velocity damping per step (0..1) so the string loses energy and settles.
        uint32_t node_count; //!< Active node count.
        uint32_t iterations; //!< Constraint relaxation iterations.
    };

    //! Compute pipeline that runs the string physics (physics.slang -> physics.spv). Owns its
    //! descriptor-set layout (two storage buffers: positions + previous positions) and pipeline
    //! layout (with the PhysicsPush push-constant range).
    class ComputePipeline {
    public:
        ComputePipeline() = default;
        ~ComputePipeline() = default;

        ComputePipeline(const ComputePipeline&) = delete;
        ComputePipeline& operator=(const ComputePipeline&) = delete;
        ComputePipeline(ComputePipeline&&) = delete;
        ComputePipeline& operator=(ComputePipeline&&) = delete;

        //! Builds the compute pipeline. Returns false and fills out_error_message on failure.
        [[nodiscard]] bool init(const Device& device, std::string& out_error_message);

        //! Releases the pipeline + layouts. Safe to call repeatedly.
        void destroy();

        [[nodiscard]] const vk::raii::Pipeline& get() const
        {
            return m_pipeline;
        }

        [[nodiscard]] const vk::raii::PipelineLayout& layout() const
        {
            return m_layout;
        }

        [[nodiscard]] const vk::raii::DescriptorSetLayout& descriptorSetLayout() const
        {
            return m_descriptor_set_layout;
        }

    private:
        vk::raii::DescriptorSetLayout m_descriptor_set_layout{nullptr}; //!< Two storage buffers.
        vk::raii::PipelineLayout m_layout{nullptr}; //!< Set layout + PhysicsPush range.
        vk::raii::Pipeline m_pipeline{nullptr}; //!< The compute pipeline.
    };

} // namespace Engine
