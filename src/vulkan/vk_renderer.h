//
// Created by user on 18.01.2024.
//

#ifndef VK_RENDERER_H
#define VK_RENDERER_H

#include "vk_descriptors.h"
// #include "renderer/renderer_frontend.h"
#include "vk_types.h"

struct deletion_queue {
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()>&& function) {
        deletors.push_back(function);
    }

    void flush() {
        // reverse iterate the deletion queue to execute all the functions
        for(auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)(); // call functors
        }

        deletors.clear();
    }
};

struct vk_frame_data {
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;

    VkSemaphore swapchain_semaphore, render_semaphore;
    VkFence render_fence;

    deletion_queue del_queue;
};

struct vk_compute_push_constants {
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

struct vk_compute_effect {
    const char* name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    vk_compute_push_constants data;
};

constexpr u32 FRAME_OVERLAP = 2;

class vk_renderer /*: public renderer*/ {
public:
    vk_renderer(void* window_ptr) {
        this->window_ptr = window_ptr;

        init_backend();
    }

    /**
     *  @brief Destroys the renderer
     */
    void destroy(); //override;

    /**
     *  @brief Renders a frame
     */
    void draw_frame(); //override;

    // // TODO: This is absolutely shit
    // static void init() {
    //     if(renderer_inst) {
    //         LOG_THROW("Renderer instance is already initialized!");
    //     }
    //
    //     renderer_inst = std::unique_ptr<renderer>(new vk_renderer());
    //     renderer_inst->init_backend();
    // }
private:
    void* window_ptr;

    VkInstance instance; // Vk Instance
    VkDebugUtilsMessengerEXT debug_messenger; // Vk debug output
    VkPhysicalDevice chosen_gpu; // Physical GPU device
    VkDevice logical_device; // Vk logical device
    VkSurfaceKHR surface; // Vk window surface

    VkSwapchainKHR swapchain; // Vk swapchain
    VkFormat swapchain_image_format; // Vk swapchain image format
    std::vector<VkImage> swapchain_images; // Vk swapchain images
    std::vector<VkImageView> swapchain_image_views; // Vk swapchain image views
    VkExtent2D swapchain_extent; // Vk swapchain extent

    u32 frame_number = 0; // Current frame number

    vk_frame_data frames[FRAME_OVERLAP];
    vk_frame_data& get_current_frame() { return frames[frame_number % FRAME_OVERLAP]; }

    VkQueue graphics_queue; // Vk graphics queue
    u32 graphics_queue_family; // Vk graphics queue family

    deletion_queue main_deletion_queue;

    VmaAllocator allocator; // VMA allocator

    // Draw resources
    vk_allocated_image draw_image;
    VkExtent2D draw_extent;

    vk_descriptor_allocator global_descriptor_allocator;

    VkDescriptorSet draw_image_descriptors;
    VkDescriptorSetLayout draw_image_descriptor_layout;

    VkPipeline gradient_pipeline;
    VkPipelineLayout gradient_pipeline_layout;

    // Immediate submit structures
    VkFence imm_fence;
    VkCommandPool imm_command_pool;
    VkCommandBuffer imm_command_buffer;

    // Compute effects
    std::vector<vk_compute_effect> background_effects;
    int current_background_effect{0};

    VkPipelineLayout triangle_pipeline_layout;
    VkPipeline triangle_pipeline;

    void init_vulkan();
    void init_swapchain();
    void init_commands();
    void init_sync_structures();
    void init_descriptors();
    void init_pipelines();
    void init_background_pipelines();
    void init_triangle_pipeline();
    void init_imgui();

    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

    void update_imgui();

    void draw_background(VkCommandBuffer cmd);
    void draw_imgui(VkCommandBuffer cmd, VkImageView target_image_view);
    void draw_geometry(VkCommandBuffer cmd);

    void create_swapchain(u32 width, u32 height);
    void destroy_swapchain();
protected:
    /**
     *  @brief Initializes the renderer
     */
    void init_backend(); //override;
};

#endif //VK_RENDERER_H
