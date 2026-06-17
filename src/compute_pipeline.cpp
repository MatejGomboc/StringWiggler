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

#include "compute_pipeline.hpp"
#include "shader_loader.hpp"
#include <array>
#include <vector>

namespace Engine
{

    bool ComputePipeline::init(const Device& device, std::string& out_error_message)
    {
        std::vector<uint32_t> spirv;
        if (!loadSpirv(executableDirectory() + "physics.spv", spirv, out_error_message)) {
            return false;
        }

        try {
            // Descriptor set layout: binding 0 = positions, binding 1 = previous positions.
            std::array<vk::DescriptorSetLayoutBinding, 2> bindings{};
            bindings[0].binding = 0;
            bindings[0].descriptorType = vk::DescriptorType::eStorageBuffer;
            bindings[0].descriptorCount = 1;
            bindings[0].stageFlags = vk::ShaderStageFlagBits::eCompute;
            bindings[1].binding = 1;
            bindings[1].descriptorType = vk::DescriptorType::eStorageBuffer;
            bindings[1].descriptorCount = 1;
            bindings[1].stageFlags = vk::ShaderStageFlagBits::eCompute;

            vk::DescriptorSetLayoutCreateInfo layout_info{};
            layout_info.setBindings(bindings);
            m_descriptor_set_layout = vk::raii::DescriptorSetLayout(device.get(), layout_info);

            vk::PushConstantRange push_range{};
            push_range.stageFlags = vk::ShaderStageFlagBits::eCompute;
            push_range.offset = 0;
            push_range.size = sizeof(PhysicsPush);

            vk::PipelineLayoutCreateInfo pipeline_layout_info{};
            pipeline_layout_info.setSetLayouts(*m_descriptor_set_layout);
            pipeline_layout_info.setPushConstantRanges(push_range);
            m_layout = vk::raii::PipelineLayout(device.get(), pipeline_layout_info);

            vk::ShaderModuleCreateInfo module_info{};
            module_info.setCode(spirv);
            vk::raii::ShaderModule module{device.get(), module_info};

            vk::PipelineShaderStageCreateInfo stage{};
            stage.stage = vk::ShaderStageFlagBits::eCompute;
            stage.module = *module;
            stage.setPName("physicsMain");

            vk::ComputePipelineCreateInfo pipeline_info{};
            pipeline_info.stage = stage;
            pipeline_info.layout = *m_layout;
            m_pipeline = vk::raii::Pipeline(device.get(), nullptr, pipeline_info);
        } catch (const vk::SystemError& e) {
            out_error_message = std::string("Vulkan error creating compute pipeline: ") + e.what();
            return false;
        } catch (const std::exception& e) {
            out_error_message = std::string("Error creating compute pipeline: ") + e.what();
            return false;
        }

        return true;
    }

    void ComputePipeline::destroy()
    {
        m_pipeline = nullptr;
        m_layout = nullptr;
        m_descriptor_set_layout = nullptr;
    }

} // namespace Engine
