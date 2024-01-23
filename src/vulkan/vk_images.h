//
// Created by user on 19.01.2024.
//

#ifndef VK_IMAGES_H
#define VK_IMAGES_H

#include "vk_types.h"

namespace vkutil {
    void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);
    void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
}

#endif //VK_IMAGES_H
