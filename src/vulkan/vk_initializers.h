//
// Created by user on 19.01.2024.
//

#ifndef VK_INITIALIZERS_H
#define VK_INITIALIZERS_H

#include "vk_types.h"

namespace vkinit {
    VkCommandPoolCreateInfo command_pool_create_info(u32 queueFamilyIndex, VkCommandPoolCreateFlags flags);
    VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, u32 count);
    VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags);
    VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags);
    VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags);
    VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspectMask);
    VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
    VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd);
    VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo);
    VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
    VkImageViewCreateInfo imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);
    VkRenderingAttachmentInfo attachment_info(VkImageView view, VkClearValue* clear ,VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo rendering_info(VkExtent2D renderExtent, VkRenderingAttachmentInfo *colorAttachment,
                                   VkRenderingAttachmentInfo *depthAttachment);
    VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage,
                                                                              VkShaderModule shader,
                                                                              const char *entry = "main");
    VkPipelineLayoutCreateInfo pipeline_layout_create_info();
} // namespace vkinit

#endif //VK_INITIALIZERS_H
