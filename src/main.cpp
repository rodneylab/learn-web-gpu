#include <GLFW/glfw3.h>
#include <fmt/core.h>
#include <webgpu/webgpu.h>

#include <iostream>

int main(int /*unused*/, char ** /*unused*/)
{
    WGPUInstanceDescriptor desc = {};
    desc.nextInChain = nullptr;
    WGPUInstance instance = wgpuCreateInstance(&desc);

    if (instance == nullptr)
    {
        std::cerr << "[ ERROR ] Could not initialise WGPU!\n";
        return 1;
    }

    fmt::print("[ INFO ] WGPU instance: {}\n", (void *)instance);

    if (glfwInit() == 0)
    {
        std::cerr << "[ ERROR ] Could not initialise GLFW!\n";
        return 1;
    }

    constexpr int kWINDOW_WIDTH(640);
    constexpr int kWINDOW_HEIGHT(480);
    GLFWwindow *window = glfwCreateWindow(kWINDOW_WIDTH,
                                          kWINDOW_HEIGHT,
                                          "Learn WebGPU",
                                          nullptr,
                                          nullptr);
    if (window == nullptr)
    {
        std::cerr << "[ ERROR ] Could not open window!\n";
        glfwTerminate();
        return 1;
    }

    while (glfwWindowShouldClose(window) == 0)
    {
        glfwPollEvents();
    }

    wgpuInstanceRelease(instance);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
