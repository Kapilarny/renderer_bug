cmake_minimum_required(VERSION 3.27)
project(vk_renderer_bug)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

add_subdirectory(external/glfw)
add_subdirectory(external/vk-bootstrap)
add_subdirectory(external/vma)
add_subdirectory(external/glm)

add_executable(vk_renderer_bug
        src/main.cpp
        src/vulkan/vk_descriptors.cpp
        src/vulkan/vk_descriptors.h
        src/vulkan/vk_images.cpp
        src/vulkan/vk_images.h
        src/vulkan/vk_initializers.cpp
        src/vulkan/vk_initializers.h
        src/vulkan/vk_pipelines.cpp
        src/vulkan/vk_pipelines.h
        src/vulkan/vk_renderer.cpp
        src/vulkan/vk_renderer.h
        src/vulkan/vk_types.h
        src/imgui/imconfig.h
        src/imgui/imgui.cpp
        src/imgui/imgui.h
        src/imgui/imgui_demo.cpp
        src/imgui/imgui_draw.cpp
        src/imgui/imgui_internal.h
        src/imgui/imgui_tables.cpp
        src/imgui/imgui_widgets.cpp
        src/imgui/imstb_rectpack.h
        src/imgui/imstb_textedit.h
        src/imgui/imstb_truetype.h
        src/imgui/backends/imgui_impl_opengl3_loader.h
        src/imgui/backends/imgui_impl_opengl3.h
        src/imgui/backends/imgui_impl_opengl3.cpp
#        src/imgui/backends/imgui_impl_win32.cpp
#        src/imgui/backends/imgui_impl_metal.h
#        src/imgui/backends/imgui_impl_win32.h
        src/imgui/backends/imgui_impl_vulkan.h
        src/imgui/backends/imgui_impl_vulkan.cpp
        src/imgui/backends/imgui_impl_glfw.h
        src/imgui/backends/imgui_impl_glfw.cpp
#        src/imgui/backends/imgui_impl_osx.h
)

target_include_directories(vk_renderer_bug PRIVATE src/ src/imgui)
target_link_libraries(vk_renderer_bug glfw Vulkan::Vulkan vk-bootstrap::vk-bootstrap GPUOpen::VulkanMemoryAllocator glm::glm)
