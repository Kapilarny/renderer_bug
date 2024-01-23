//
// Created by user on 19.01.2024.
//

#ifndef VK_DESCRIPTORS_H
#define VK_DESCRIPTORS_H

#include "vk_types.h"

struct vk_descriptor_allocator {
    struct pool_size_ratio {
        VkDescriptorType type;
        float ratio;
    };

    VkDescriptorPool pool;

    void init_pool(VkDevice device, u32 max_sets, std::span<pool_size_ratio> pool_ratios);
    void clear_descriptors(VkDevice device);
    void destroy_pool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};

struct vk_descriptor_layout_builder {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(u32 binding, VkDescriptorType type);
    void clear();

    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shader_stages);
};

#endif //VK_DESCRIPTORS_H
