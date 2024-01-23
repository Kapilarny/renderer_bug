//
// Created by user on 19.01.2024.
//

#ifndef VK_PIPELINES_H
#define VK_PIPELINES_H

#include "vk_types.h"

class vk_pipeline_builder {
public:
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    VkPipelineMultisampleStateCreateInfo multisampling;
    VkPipelineLayout pipeline_layout;
    VkPipelineDepthStencilStateCreateInfo depth_stencil;
    VkPipelineRenderingCreateInfo render_info;
    VkFormat color_attachment_format;

    vk_pipeline_builder() { clear(); }

    void set_shaders(VkShaderModule vert_shader, VkShaderModule frag_shader);
    void set_input_topology(VkPrimitiveTopology topology);
    void set_polygon_mode(VkPolygonMode mode);
    void set_cull_mode(VkCullModeFlags mode, VkFrontFace front_face);
    void set_multisampling_none();
    void disable_blending();
    void set_color_attachment_format(VkFormat format);
    void set_depth_format(VkFormat format);
    void disable_depthtest();

    void clear();

    VkPipeline build_pipeline(VkDevice device);
};

namespace vkutil {
    bool load_shader_module(const char* file_path, VkDevice device, VkShaderModule* out_shader_module);
}

#endif //VK_PIPELINES_H
