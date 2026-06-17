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

#include "pipeline.hpp"
#include "shader_loader.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace Engine
{

    bool Pipeline::init(const Device& device, vk::Format colour_format, std::string& out_error_message)
    {
        std::vector<uint32_t> spirv;
        if (!loadSpirv(executableDirectory() + "line.spv", spirv, out_error_message)) {
            return false;
        }

        try {
            vk::ShaderModuleCreateInfo module_info{};
            module_info.setCode(spirv);
            vk::raii::ShaderModule module{device.get(), module_info};

            // Both entry points live in the one module (slangc -entry vertMain -entry fragMain).
            std::array<vk::PipelineShaderStageCreateInfo, 2> stages{};
            stages[0].stage = vk::ShaderStageFlagBits::eVertex;
            stages[0].module = *module;
            stages[0].setPName("vertMain");
            stages[1].stage = vk::ShaderStageFlagBits::eFragment;
            stages[1].module = *module;
            stages[1].setPName("fragMain");

            // Vertex input: one binding of tightly-packed Vec2 positions at location 0.
            vk::VertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(float) * 2;
            binding.inputRate = vk::VertexInputRate::eVertex;

            vk::VertexInputAttributeDescription attribute{};
            attribute.location = 0;
            attribute.binding = 0;
            attribute.format = vk::Format::eR32G32Sfloat;
            attribute.offset = 0;

            vk::PipelineVertexInputStateCreateInfo vertex_input{};
            vertex_input.setVertexBindingDescriptions(binding);
            vertex_input.setVertexAttributeDescriptions(attribute);

            vk::PipelineInputAssemblyStateCreateInfo input_assembly{};
            input_assembly.topology = vk::PrimitiveTopology::eLineStrip;
            input_assembly.primitiveRestartEnable = vk::False;

            std::array<vk::DynamicState, 2> dynamic_states{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
            vk::PipelineDynamicStateCreateInfo dynamic_state{};
            dynamic_state.setDynamicStates(dynamic_states);

            vk::PipelineViewportStateCreateInfo viewport_state{};
            viewport_state.viewportCount = 1;
            viewport_state.scissorCount = 1;

            vk::PipelineRasterizationStateCreateInfo rasterisation{};
            rasterisation.depthClampEnable = vk::False;
            rasterisation.rasterizerDiscardEnable = vk::False;
            rasterisation.polygonMode = vk::PolygonMode::eFill;
            rasterisation.cullMode = vk::CullModeFlagBits::eNone;
            rasterisation.frontFace = vk::FrontFace::eCounterClockwise;
            rasterisation.depthBiasEnable = vk::False;
            rasterisation.lineWidth = 1.0f;

            vk::PipelineMultisampleStateCreateInfo multisample{};
            multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;
            multisample.sampleShadingEnable = vk::False;

            vk::PipelineColorBlendAttachmentState blend_attachment{};
            blend_attachment.blendEnable = vk::False;
            blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
                | vk::ColorComponentFlagBits::eA;

            vk::PipelineColorBlendStateCreateInfo colour_blend{};
            colour_blend.logicOpEnable = vk::False;
            colour_blend.setAttachments(blend_attachment);

            vk::PipelineLayoutCreateInfo layout_info{}; // empty: no descriptor sets or push constants yet.
            m_layout = vk::raii::PipelineLayout(device.get(), layout_info);

            // Dynamic rendering: declare the colour attachment format instead of a render pass.
            vk::PipelineRenderingCreateInfo rendering_info{};
            rendering_info.setColorAttachmentFormats(colour_format);

            vk::GraphicsPipelineCreateInfo pipeline_info{};
            pipeline_info.setStages(stages);
            pipeline_info.pVertexInputState = &vertex_input;
            pipeline_info.pInputAssemblyState = &input_assembly;
            pipeline_info.pViewportState = &viewport_state;
            pipeline_info.pRasterizationState = &rasterisation;
            pipeline_info.pMultisampleState = &multisample;
            pipeline_info.pColorBlendState = &colour_blend;
            pipeline_info.pDynamicState = &dynamic_state;
            pipeline_info.layout = *m_layout;
            pipeline_info.renderPass = nullptr; // using dynamic rendering
            pipeline_info.setPNext(&rendering_info);

            m_pipeline = vk::raii::Pipeline(device.get(), nullptr, pipeline_info);
        } catch (const vk::SystemError& e) {
            out_error_message = std::string("Vulkan error creating pipeline: ") + e.what();
            return false;
        } catch (const std::exception& e) {
            out_error_message = std::string("Error creating pipeline: ") + e.what();
            return false;
        }

        return true;
    }

    void Pipeline::destroy()
    {
        m_pipeline = nullptr;
        m_layout = nullptr;
    }

} // namespace Engine
