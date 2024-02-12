#include "webgpu-utils.h"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <spdlog/spdlog.h>
#include <webgpu/webgpu.h>

#include <iostream>

int main(int /*unused*/, char ** /*unused*/)
{
    WGPUInstanceDescriptor desc = {};
    desc.nextInChain = nullptr;
    WGPUInstance instance{wgpuCreateInstance(&desc)};

    if (instance == nullptr)
    {
        spdlog::error("[ ERROR ] Could not initialise WGPU!");
        return 1;
    }

    spdlog::info("WGPU instance: {}", (void *)instance);

    if (glfwInit() == 0)
    {

        spdlog::error("[ ERROR ] Could not initialise GLFW!");
        return 1;
    }

    constexpr int kWINDOW_WIDTH{640};
    constexpr int kWINDOW_HEIGHT{480};
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(kWINDOW_WIDTH,
                                          kWINDOW_HEIGHT,
                                          "Learn WebGPU",
                                          nullptr,
                                          nullptr);
    if (window == nullptr)
    {
        spdlog::error("[ ERROR ] Could not open window!");
        glfwTerminate();
        return 1;
    }

    spdlog::info("Requesting adapter...");
    // Utility function provided by glfw3webgpu.h
    WGPUSurface surface = glfwGetWGPUSurface(instance, window);
    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    adapterOpts.compatibleSurface = surface;
    WGPUAdapter adapter = requestAdapter(instance, &adapterOpts);
    spdlog::info("Got adapter: {}", (void *)adapter);

    inspectAdapter(adapter);

    spdlog::info("Requesting device...");

    WGPUDeviceDescriptor deviceDesc{};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "My Device";
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredLimits = nullptr;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The default queue";
    WGPUDevice device = requestDevice(adapter, &deviceDesc);
    spdlog::info("Got device: {}\n", (void *)device);

    auto onDeviceError =
        [](WGPUErrorType type, char const *message, void * /* pUserData */) {
            if (message != nullptr)
            {
                spdlog::error("Uncaptured device error: type {} ({})",
                              type,
                              message);
            }
            else
            {
                spdlog::error("Uncaptured device error: type {}", type);
            }
        };
    wgpuDeviceSetUncapturedErrorCallback(device,
                                         onDeviceError,
                                         nullptr /* pUserData */);

    inspectDevice(device);

    while (glfwWindowShouldClose(window) == 0)
    {
        glfwPollEvents();
    }

    wgpuDeviceRelease(device);
    wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);
    wgpuSurfaceRelease(surface);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
