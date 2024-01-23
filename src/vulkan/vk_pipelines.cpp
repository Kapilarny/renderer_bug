//
// Created by user on 19.01.2024.
//

#include "vk_pipelines.h"

#include <fstream>


#include "vk_initializers.h"

bool vkutil::load_shader_module(const char *file_path, VkDevice device, VkShaderModule *out_shader_module) {
    // open the file, with cursor at the end
    std::ifstream file(file_path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    u64 file_size = (u64) file.tellg();

    // spirv expects the buffer to be on u32 so make sure to reserve enough space
    std::vector<u32> buffer(file_size / sizeof(u32));

    file.seekg(0);
    file.read((char *) buffer.data(), file_size);
    file.close();

    // Create the shader module
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = buffer.size() * sizeof(u32);
    create_info.pCode = buffer.data();

    create_info.pNext = nullptr;

    // check if the creation was successful
    VkShaderModule shader_module;
    if (vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
        return false;
    }

    *out_shader_module = shader_module;
    return true;
}

void vk_pipeline_builder::set_shaders(VkShaderModule vert_shader, VkShaderModule frag_shader) {
    shader_stages.clear();

    shader_stages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vert_shader));
    shader_stages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, frag_shader));
}

void vk_pipeline_builder::set_input_topology(VkPrimitiveTopology topology) {
    input_assembly.topology = topology;

    // We are not goint to use primitive restart
    input_assembly.primitiveRestartEnable = VK_FALSE;
}

void vk_pipeline_builder::set_polygon_mode(VkPolygonMode mode) {
    rasterizer.polygonMode = mode;
    rasterizer.lineWidth = 1.f;
}

void vk_pipeline_builder::set_cull_mode(VkCullModeFlags mode, VkFrontFace front_face) {
    rasterizer.cullMode = mode;
    rasterizer.frontFace = front_face;
}

void vk_pipeline_builder::set_multisampling_none() {
    multisampling.sampleShadingEnable = VK_FALSE;

    // multisampling defaulted to no multisampling (1 sample per pixel)
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.f;
    multisampling.pSampleMask = nullptr;

    // No alpha-to-coverage either
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;
}

void vk_pipeline_builder::disable_blending() {
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // No blending
    color_blend_attachment.blendEnable = VK_FALSE;
}

void vk_pipeline_builder::set_color_attachment_format(VkFormat format) {
    color_attachment_format = format;

    // Connect the format to the render_info
    render_info.colorAttachmentCount = 1;
    render_info.pColorAttachmentFormats = &color_attachment_format;
}

void vk_pipeline_builder::set_depth_format(VkFormat format) {
    render_info.depthAttachmentFormat = format;
}

void vk_pipeline_builder::disable_depthtest() {
    depth_stencil.depthTestEnable = VK_FALSE;
    depth_stencil.depthWriteEnable = VK_FALSE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;
    depth_stencil.front = {};
    depth_stencil.back = {};
    depth_stencil.minDepthBounds = 0.f;
    depth_stencil.maxDepthBounds = 1.f;
}

void vk_pipeline_builder::clear() {
    // Clear all the structures
    input_assembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    color_blend_attachment = {};
    multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    depth_stencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    render_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO  };
    pipeline_layout = {};

    shader_stages.clear();
}

VkPipeline vk_pipeline_builder::build_pipeline(VkDevice device) {
    // Make viewport state from our stored viewport and scissor.
    // At the moment we won't support multiple viewports or scissors
    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.pNext = nullptr;

    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    // Setup dummy color blending. We aren't using transparent objects yet the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo color_blending = {};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.pNext = nullptr;

    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    // Completely clear VertexxInputStateCreateInfo, no need for it yet
    VkPipelineVertexInputStateCreateInfo vertex_input_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    // Build the actual pipeline
    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    // Connect the render_info with the pipeline
    render_info.pNext = &render_info;

    pipeline_info.stageCount = shader_stages.size();
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.layout = pipeline_layout;

    VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamic_info.dynamicStateCount = 2;
    dynamic_info.pDynamicStates = state;

    pipeline_info.pDynamicState = &dynamic_info;

    // It's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK
    VkPipeline new_pipeline;
    if(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &new_pipeline) != VK_SUCCESS) {
        LOG_FATAL("Failed to create graphics pipeline!");
        return VK_NULL_HANDLE;
    }

    return new_pipeline;
}
