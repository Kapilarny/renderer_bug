#include <iostream>

#include <GLFW/glfw3.h>

#include "vulkan/vk_renderer.h"

int main()
{
    // Create a Window
    if(!glfwInit()) {
        throw std::runtime_error("Failed to init GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* glfw_window = glfwCreateWindow(800, 600, "vk_renderer_bug", 0, 0);
    if(!glfw_window) {
        throw std::runtime_error("Failed to create a window!");
    }

    glfwMakeContextCurrent(glfw_window);

    std::cout << "What is going on";

    vk_renderer renderer = vk_renderer(glfw_window);

    while(!glfwWindowShouldClose(glfw_window)) {
        glfwPollEvents();

        // Run render
        renderer.draw_frame();
    }

    renderer.destroy();

    return 0;
}
