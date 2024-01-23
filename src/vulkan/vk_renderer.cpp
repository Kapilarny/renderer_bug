//
// Created by user on 18.01.2024.
//

#include "vk_renderer.h"

#include <chrono>
#include <thread>

#define VMA_IMPLEMENTATION
#include <imgui/backends/imgui_impl_glfw.h>
#include <vk_mem_alloc.h>

#include "VkBootstrap.h"
#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
// #include "window.h"

#include <iostream>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_vulkan.h"

#include <GLFW/glfw3.h>

#define SHOULD_USE_VALIDATION_LAYERS true

// Im lazy
#define LOG_DEBUG(s) std::cout << s << "\n";

// TODO: Organize this file

void vk_renderer::init_backend() {
    init_vulkan();

    // Initialize swapchain
    init_swapchain();

    // Initialize the frames
    init_commands();

    // Initialize synchronization structures
    init_sync_structures();

    // Initialize descriptors
    init_descriptors();

    // Initialize pipelines
    init_pipelines();

    // Initialize imgui
    init_imgui();

    LOG_DEBUG("Vulkan Backend successfully initialized!");
}

void vk_renderer::destroy() {
    // if(!renderer_inst) return;

    // Wait for the device to finish all operations before destroying
    vkDeviceWaitIdle(logical_device);

    main_deletion_queue.flush();

    for(auto& frame : frames) {
        vkDestroyCommandPool(logical_device, frame.command_pool, nullptr);

        vkDestroyFence(logical_device, frame.render_fence, nullptr);
        vkDestroySemaphore(logical_device, frame.swapchain_semaphore, nullptr);
        vkDestroySemaphore(logical_device, frame.render_semaphore, nullptr);

        frame.del_queue.flush();
    }

    destroy_swapchain();

    vkDestroySurfaceKHR(instance, surface, nullptr);

    vkDestroyDevice(logical_device, nullptr);
    vkb::destroy_debug_utils_messenger(instance, debug_messenger);
    vkDestroyInstance(instance, nullptr);

    // renderer_inst.reset();
}

void vk_renderer::draw_frame() {
    // if(!should_render) return;

    // TODO: Throw out ImGui from here

    // Draw ImGui
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Update the UI
    update_imgui();

    ImGui::Render();

    // Wait for the gpu to finish rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(logical_device, 1, &get_current_frame().render_fence, true, 100000000));

    get_current_frame().del_queue.flush();

    u32 swapchain_image_index;
    VK_CHECK(vkAcquireNextImageKHR(logical_device, swapchain, 100000000, get_current_frame().swapchain_semaphore, nullptr, &swapchain_image_index));

    VK_CHECK(vkResetFences(logical_device, 1, &get_current_frame().render_fence));

    VkCommandBuffer cmd = get_current_frame().command_buffer;

    // Now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    // Begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    draw_extent.width = draw_image.image_extent.width;
    draw_extent.height = draw_image.image_extent.height;

    // Begin the command buffer
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    vkutil::transition_image(cmd, draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    draw_background(cmd);

    vkutil::transition_image(cmd, draw_image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    draw_geometry(cmd);

    //transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::transition_image(cmd, draw_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // execute a copy from the draw image into the swapchain
    vkutil::copy_image_to_image(cmd, draw_image.image, swapchain_images[swapchain_image_index], draw_extent, swapchain_extent);

    // set swapchain image layout to Attachment Optimal so we can draw it
    vkutil::transition_image(cmd, swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //draw imgui into the swapchain image
    draw_imgui(cmd, swapchain_image_views[swapchain_image_index]);

    // set swapchain image layout to Present so we can draw it
    vkutil::transition_image(cmd, swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    // Prepare the submission to the queue
    // We want to wait on the present_semaphore, as that semaphore is signaled when the swapchain is ready
    // We will signal the render_semaphore, to signal that rendering has finished
    VkCommandBufferSubmitInfo cmd_submit_info = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo signalSemaphoreSubmitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().render_semaphore);
    VkSemaphoreSubmitInfo waitSemaphoreSubmitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, get_current_frame().swapchain_semaphore);

    VkSubmitInfo2 submit_info = vkinit::submit_info(&cmd_submit_info, &signalSemaphoreSubmitInfo, &waitSemaphoreSubmitInfo);

    // Submit command buffer to the queue and execute it.
    // render_fence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(graphics_queue, 1, &submit_info, get_current_frame().render_fence));

    // Prepare present
    // This will put the image we just rendered to into the visible window.
    // We want to wait on the render_semaphore for that, as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = nullptr;
    present_info.pSwapchains = &swapchain;
    present_info.swapchainCount = 1;

    present_info.pWaitSemaphores = &get_current_frame().render_semaphore;
    present_info.waitSemaphoreCount = 1;

    present_info.pImageIndices = &swapchain_image_index;

    VK_CHECK(vkQueuePresentKHR(graphics_queue, &present_info));

    frame_number++;
}

void vk_renderer::init_vulkan() {
    // Create the instance
    vkb::InstanceBuilder builder;

    auto inst_ret = builder.set_app_name("YeClient")
    // Add synchronization layers
            .request_validation_layers(SHOULD_USE_VALIDATION_LAYERS)
            // .add_validation_feature_enable(VK)
            .require_api_version(1, 3, 0)
            .use_default_debug_messenger()
            .build();

    vkb::Instance vkb_inst = inst_ret.value();

    instance = vkb_inst.instance;
    debug_messenger = vkb_inst.debug_messenger;

    // Create the surface
    if(glfwCreateWindowSurface(instance, (GLFWwindow*)window_ptr, nullptr, &surface) != VK_SUCCESS) {
        LOG_THROW("Failed to create the window surface!");
    }

    // Create the physical and logical device

    // Vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features vk13_features = {};
    vk13_features.dynamicRendering = true;
    vk13_features.synchronization2 = true;

    // Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features vk12_features = {};
    vk12_features.bufferDeviceAddress = true;
    vk12_features.descriptorIndexing = true;

    // Use vkbootstrap to select a gpu.
    // We want a gpu that can write to the surface and supports vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector selector{vkb_inst};
    vkb::PhysicalDevice physical_device = selector
            .set_minimum_version(1, 3)
            .set_surface(surface)
            .set_required_features_13(vk13_features)
            .set_required_features_12(vk12_features)
            .select()
            .value();

    vkb::DeviceBuilder device_builder{physical_device};
    vkb::Device vkb_device = device_builder.build().value();

    logical_device = vkb_device.device;
    chosen_gpu = physical_device.physical_device;

    // Get the driver properties
    VkPhysicalDeviceProperties gpu_properties;
    vkGetPhysicalDeviceProperties(chosen_gpu, &gpu_properties);

    // Print the driver name
    LOG_INFO("GPU:");
    LOG_INFO("- Using GPU: %s", gpu_properties.deviceName);


    // Get the graphics queue
    graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
    graphics_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    // Initialize the allocator
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.physicalDevice = chosen_gpu;
    allocator_info.device = logical_device;
    allocator_info.instance = instance;
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocator_info, &allocator);

    main_deletion_queue.push_function([&]() {
        vmaDestroyAllocator(allocator);
    });
}

void vk_renderer::init_swapchain() {
    auto width = 800;
    auto height = 600;

    create_swapchain(width, height);

    // Draw image size will match the window size
    VkExtent3D draw_img_extent = {width, height, 1};

    // HARDCODING: draw format to 32bit float
    draw_image.image_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    draw_image.image_extent = draw_img_extent;

    VkImageUsageFlags draw_image_usages{};
    draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo img_info = vkinit::image_create_info(draw_image.image_format, draw_image_usages, draw_img_extent);

    // For the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo img_alloc_info = {};
    img_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    img_alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Allocate and create the image
    vmaCreateImage(allocator, &img_info, &img_alloc_info, &draw_image.image, &draw_image.allocation, nullptr);

    // Build an image-view for the draw image to use for rendering
    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(draw_image.image_format, draw_image.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(logical_device, &view_info, nullptr, &draw_image.image_view));

    // Add to the deletion queue
    main_deletion_queue.push_function([=]() {
        vkDestroyImageView(logical_device, draw_image.image_view, nullptr);
        vmaDestroyImage(allocator, draw_image.image, draw_image.allocation);
    });
}

void vk_renderer::init_commands() {
    // Create a command pool for commands submitted to the graphics queue
    // We also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo pool_info = vkinit::command_pool_create_info(graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for(auto& frame : frames) {
        VK_CHECK(vkCreateCommandPool(logical_device, &pool_info, nullptr, &frame.command_pool));

        // Allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmd_buffer_info = vkinit::command_buffer_allocate_info(frame.command_pool, 1);

        VK_CHECK(vkAllocateCommandBuffers(logical_device, &cmd_buffer_info, &frame.command_buffer));
    }

    // Add immediate submit structures
    VK_CHECK(vkCreateCommandPool(logical_device, &pool_info, nullptr, &imm_command_pool));

    // Allocate the cmd buffer for immediate submits
    VkCommandBufferAllocateInfo cmd_buffer_info = vkinit::command_buffer_allocate_info(imm_command_pool, 1);

    VK_CHECK(vkAllocateCommandBuffers(logical_device, &cmd_buffer_info, &imm_command_buffer));

    main_deletion_queue.push_function([=]() {
        vkDestroyCommandPool(logical_device, imm_command_pool, nullptr);
    });
}

void vk_renderer::init_sync_structures() {
    // Create synchronization structures
    // One fence to control when the gpu has finished rendering the frame
    // And two semaphores to synchronize rendering with swapchain operations
    // We want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo fence_info = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphore_info = vkinit::semaphore_create_info(0);

    for(auto& frame : frames) {
        VK_CHECK(vkCreateFence(logical_device, &fence_info, nullptr, &frame.render_fence));
        VK_CHECK(vkCreateSemaphore(logical_device, &semaphore_info, nullptr, &frame.swapchain_semaphore));
        VK_CHECK(vkCreateSemaphore(logical_device, &semaphore_info, nullptr, &frame.render_semaphore));
    }

    // Immediate submit structures
    VK_CHECK(vkCreateFence(logical_device, &fence_info, nullptr, &imm_fence));
    main_deletion_queue.push_function([=]() { vkDestroyFence(logical_device, imm_fence, nullptr); });
}

void vk_renderer::init_descriptors() {
    // Create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<vk_descriptor_allocator::pool_size_ratio> pool_sizes = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
    };

    global_descriptor_allocator.init_pool(logical_device, 10, pool_sizes);

    // Make the descriptor set layout for our compute draw
    vk_descriptor_layout_builder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    draw_image_descriptor_layout = builder.build(logical_device, VK_SHADER_STAGE_COMPUTE_BIT);

    // Allocate a descriptor set for our draw image
    draw_image_descriptors = global_descriptor_allocator.allocate(logical_device, draw_image_descriptor_layout);

    VkDescriptorImageInfo img_info{};
    img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    img_info.imageView = draw_image.image_view;

    VkWriteDescriptorSet draw_image_write{};
    draw_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    draw_image_write.pNext = nullptr;

    draw_image_write.dstBinding = 0;
    draw_image_write.dstSet = draw_image_descriptors;
    draw_image_write.descriptorCount = 1;
    draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    draw_image_write.pImageInfo = &img_info;

    vkUpdateDescriptorSets(logical_device, 1, &draw_image_write, 0, nullptr);

    // Add to the deletion queue
    main_deletion_queue.push_function([=]() {
        vkDestroyDescriptorSetLayout(logical_device, draw_image_descriptor_layout, nullptr);
        global_descriptor_allocator.destroy_pool(logical_device);
    });
}

void vk_renderer::init_pipelines() {
    init_background_pipelines();
    init_triangle_pipeline();
}

void vk_renderer::init_background_pipelines() {
    VkPipelineLayoutCreateInfo compute_layout{};
    compute_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    compute_layout.pNext = nullptr;
    compute_layout.setLayoutCount = 1;
    compute_layout.pSetLayouts = &draw_image_descriptor_layout;

    VkPushConstantRange push_constant{};
    push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_constant.offset = 0;
    push_constant.size = sizeof(vk_compute_push_constants);

    compute_layout.pPushConstantRanges = &push_constant;
    compute_layout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(logical_device, &compute_layout, nullptr, &gradient_pipeline_layout));

    VkShaderModule gradient_shader;
    if (!vkutil::load_shader_module("../shaders/gradient_color.comp.spv", logical_device, &gradient_shader)) {
        // Print the working directory
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        LOG_INFO("Working directory: %s", cwd);

        LOG_THROW("Failed to load compute shader");
    }

    VkShaderModule sky_shader;
    if (!vkutil::load_shader_module("../shaders/sky.comp.spv", logical_device, &sky_shader)) {
        // Print the working directory
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        LOG_INFO("Working directory: %s", cwd);

        LOG_THROW("Failed to load compute shader");
    }

    VkPipelineShaderStageCreateInfo stage_info{};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.pNext = nullptr;
    stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = gradient_shader;
    stage_info.pName = "main";

    VkComputePipelineCreateInfo compute_pipeline_create_info{};
    compute_pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_create_info.pNext = nullptr;
    compute_pipeline_create_info.layout = gradient_pipeline_layout;
    compute_pipeline_create_info.stage = stage_info;


    // Create the gradient compute effect
    vk_compute_effect gradient{};
    gradient.layout = gradient_pipeline_layout;
    gradient.name = "gradient";
    gradient.data = {};

    // default colors
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);

    VK_CHECK(vkCreateComputePipelines(logical_device, nullptr, 1, &compute_pipeline_create_info, nullptr,
                                      &gradient.pipeline));

    // Create the Sky

    // Change the shader module to create the sky shader
    compute_pipeline_create_info.stage.module = sky_shader;

    vk_compute_effect sky{};
    sky.layout = gradient_pipeline_layout;
    sky.name = "sky";
    sky.data = {};

    // Default sky params
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    VK_CHECK(vkCreateComputePipelines(logical_device, nullptr, 1, &compute_pipeline_create_info, nullptr,
                                      &sky.pipeline));

    // Add background effects to the list
    background_effects.push_back(gradient);
    background_effects.push_back(sky);

    vkDestroyShaderModule(logical_device, gradient_shader, nullptr);
    vkDestroyShaderModule(logical_device, sky_shader, nullptr);

    main_deletion_queue.push_function([&]() {
        vkDestroyPipelineLayout(logical_device, gradient_pipeline_layout, nullptr);

        for (const auto &bg_effect: background_effects) {
            vkDestroyPipeline(logical_device, bg_effect.pipeline, nullptr);
        }
    });
}

void vk_renderer::init_triangle_pipeline() {
    VkShaderModule triangle_vert_shader;
    if(!vkutil::load_shader_module("../shaders/colored_triangle.vert.spv", logical_device, &triangle_vert_shader)) {
        // Print the working directory
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        LOG_INFO("Working directory: %s", cwd);

        LOG_THROW("Failed to load vertex shader");
    }

    VkShaderModule triangle_frag_shader;
    if(!vkutil::load_shader_module("../shaders/colored_triangle.frag.spv", logical_device, &triangle_frag_shader)) {
        // Print the working directory
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        LOG_INFO("Working directory: %s", cwd);

        LOG_THROW("Failed to load fragment shader");
    }

    // Build the pipeline layout that controls the inputs/outputs of the shader
    // We are not using descriptor sets or other fancy stuff yet, so this can be empty for now
    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(logical_device, &pipeline_layout_info, nullptr, &triangle_pipeline_layout));

    vk_pipeline_builder pipeline_builder;

    pipeline_builder.pipeline_layout = triangle_pipeline_layout;
    pipeline_builder.set_shaders(triangle_vert_shader, triangle_frag_shader);
    pipeline_builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipeline_builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder.set_multisampling_none();
    pipeline_builder.disable_blending();
    pipeline_builder.disable_depthtest();

    // Connect the image format we will draw into
    pipeline_builder.set_color_attachment_format(draw_image.image_format);
    pipeline_builder.set_depth_format(VK_FORMAT_UNDEFINED);

    // Finally build the pipeline
    triangle_pipeline = pipeline_builder.build_pipeline(logical_device);

    // Cleanup
    vkDestroyShaderModule(logical_device, triangle_vert_shader, nullptr);
    vkDestroyShaderModule(logical_device, triangle_frag_shader, nullptr);

    main_deletion_queue.push_function([&]() {
        vkDestroyPipelineLayout(logical_device, triangle_pipeline_layout, nullptr);
        vkDestroyPipeline(logical_device, triangle_pipeline, nullptr);
    });
}

void vk_renderer::init_imgui() {
    // 1. Create Descriptor Pool for IMGUI
    // The size of the pool is very oversize, but it's copied from the imgui example itself
    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (u32)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imgui_pool;
    VK_CHECK(vkCreateDescriptorPool(logical_device, &pool_info, nullptr, &imgui_pool));

    // 2. Initialize imgui
    ImGui::CreateContext();

    // Initialize ImGui for our window
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)window_ptr, true);

    // Initiialize for vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = chosen_gpu;
    init_info.Device = logical_device;
    init_info.QueueFamily = graphics_queue_family;
    init_info.Queue = graphics_queue;
    init_info.DescriptorPool = imgui_pool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;
    init_info.ColorAttachmentFormat = swapchain_image_format;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);

    // Add to the deletion queue
    main_deletion_queue.push_function([=]() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(logical_device, imgui_pool, nullptr);
    });
}

void vk_renderer::immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function) {
    VK_CHECK(vkResetFences(logical_device, 1, &imm_fence));
    VK_CHECK(vkResetCommandBuffer(imm_command_buffer, 0));

    VkCommandBuffer cmd = imm_command_buffer;

    VkCommandBufferBeginInfo cmdBeginInfo =
            vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmd_submit_info = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit_info = vkinit::submit_info(&cmd_submit_info, nullptr, nullptr);

    // Submit command buffer to the queue and execute it.
    // render_fence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(graphics_queue, 1, &submit_info, imm_fence));

    VK_CHECK(vkWaitForFences(logical_device, 1, &imm_fence, true, 1000000000));
}

void vk_renderer::update_imgui() {
    if (ImGui::Begin("background")) {
        vk_compute_effect& selected = background_effects[current_background_effect];

        ImGui::Text("Selected effect: ", selected.name);

        ImGui::SliderInt("Effect Index", &current_background_effect, 0, background_effects.size() - 1);

        ImGui::InputFloat4("data1",(float*)& selected.data.data1);
        ImGui::InputFloat4("data2",(float*)& selected.data.data2);
        ImGui::InputFloat4("data3",(float*)& selected.data.data3);
        ImGui::InputFloat4("data4",(float*)& selected.data.data4);
    }

    ImGui::End();
}

void vk_renderer::create_swapchain(u32 width, u32 height) {
    vkb::SwapchainBuilder swapchainBuilder{chosen_gpu, logical_device, surface};

    swapchain_image_format = VK_FORMAT_B8G8R8A8_SRGB;

    vkb::Swapchain vkb_swapchain = swapchainBuilder
            .set_desired_format(VkSurfaceFormatKHR{ .format = swapchain_image_format, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value();

    swapchain_extent = vkb_swapchain.extent;
    swapchain = vkb_swapchain.swapchain;
    swapchain_images = vkb_swapchain.get_images().value();
    swapchain_image_views = vkb_swapchain.get_image_views().value();
}

void vk_renderer::destroy_swapchain() {
    vkDestroySwapchainKHR(logical_device, swapchain, nullptr);

    // Destroy the image views
    for(auto& image_view : swapchain_image_views) {
        vkDestroyImageView(logical_device, image_view, nullptr);
    }
}

void vk_renderer::draw_background(VkCommandBuffer cmd) {
    vk_compute_effect& effect = background_effects[current_background_effect];

    // Bind the gradient drawing compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // Bind the descriptor set for the draw image
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradient_pipeline_layout, 0, 1,
                            &draw_image_descriptors, 0, nullptr);

    vkCmdPushConstants(cmd, gradient_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vk_compute_push_constants), &effect.data);

    // execute the compute pipeline dispatch. We are using 16x16 work group so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(draw_extent.width / 16), std::ceil(draw_extent.width / 16), 1);
}

void vk_renderer::draw_imgui(VkCommandBuffer cmd, VkImageView target_image_view) {
    VkRenderingAttachmentInfo color_attachment = vkinit::attachment_info(target_image_view, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(swapchain_extent, &color_attachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void vk_renderer::draw_geometry(VkCommandBuffer cmd) {
    //begin a render pass  connected to our draw image
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(draw_image.image_view, nullptr, VK_IMAGE_LAYOUT_GENERAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(draw_extent, &colorAttachment, nullptr);
    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, triangle_pipeline);

    //set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = draw_extent.width;
    viewport.height = draw_extent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = draw_extent.width;
    scissor.extent.height = draw_extent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    //launch a draw command to draw 3 vertices
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRendering(cmd);
}