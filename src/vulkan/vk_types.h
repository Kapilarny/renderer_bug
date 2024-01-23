//
// Created by user on 18.01.2024.
//

#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <iostream>
#include <exception>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "defines.h"

// #include <logger.h>

#define LOG_INFO(s, ...) std::cout << s << "\n";
#define LOG_FATAL(s, ...) LOG_INFO(s);
#define LOG_THROW(s, ...) throw std::runtime_error(s);

#define VK_CHECK(x) { \
    VkResult res = x; \
    if (res != VK_SUCCESS) { \
        throw std::runtime_error(string_VkResult(res)); \
    } \
}

struct vk_allocated_image {
    VkImage image;
    VkImageView image_view;
    VmaAllocation allocation;
    VkExtent3D image_extent;
    VkFormat image_format;
};

#endif //VK_TYPES_H
