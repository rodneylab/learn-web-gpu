#include "webgpu-utils.h"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <spdlog/spdlog.h>
//#include <webgpu/webgpu.h>
#include <webgpu/webgpu.hpp>

#include <iostream>

int main(int /*unused*/, char ** /*unused*/)
{
    wgpu::InstanceDescriptor desc = {};
    desc.nextInChain = nullptr;
    wgpu::Instance instance{wgpuCreateInstance(&desc)};

    if (instance == nullptr)
    {
        spdlog::error("Could not initialise WGPU!");
        return 1;
    }

    spdlog::info("WGPU instance: {}", (void *)instance);

    if (glfwInit() == 0)
    {

        spdlog::error("Could not initialise GLFW!");
        return 1;
    }

    constexpr int kWindowWidth{640};
    constexpr int kWindowHeight{480};
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(kWindowWidth,
                                          kWindowHeight,
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
    wgpu::Surface surface = glfwGetWGPUSurface(instance, window);
    wgpu::RequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    adapterOpts.compatibleSurface = surface;
    wgpu::Adapter adapter = requestAdapter(instance, &adapterOpts);
    spdlog::info("Got adapter: {}", (void *)adapter);

    inspectAdapter(adapter);

    spdlog::info("Requesting device...");

    wgpu::DeviceDescriptor deviceDesc{};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "My Device";
    deviceDesc.requiredFeaturesCount = 0;
    //deviceDesc.requiredFeatureCount = 0; // newer version
    deviceDesc.requiredLimits = nullptr;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The default queue";
    wgpu::Device device = requestDevice(adapter, &deviceDesc);
    spdlog::info("Got device: {}\n", (void *)device);

    wgpu::Queue queue = wgpuDeviceGetQueue(device);

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

    //    For use with newer version, from which Swap Chain has been removed
    //    wgpu::SurfaceConfiguration surfaceConfiguration{};
    //    surfaceConfiguration.nextInChain = nullptr;
    //    surfaceConfiguration.device = device;
    //    surfaceConfiguration.format =
    //        wgpuSurfaceGetPreferredFormat(surface, adapter);
    //    surfaceConfiguration.usage = wgpu::TextureUsage_RenderAttachment;
    //    surfaceConfiguration.viewFormatCount = 0;
    //    surfaceConfiguration.viewFormats = nullptr;
    //    surfaceConfiguration.alphaMode = wgpu::CompositeAlphaMode_Auto;
    //    surfaceConfiguration.width = kWindowWidth;
    //    surfaceConfiguration.height = kWindowHeight;
    //    surfaceConfiguration.presentMode = wgpu::PresentMode_Fifo;
    //    wgpuSurfaceConfigure(surface, &surfaceConfiguration);


    // This Swap Chain code will need updating for latet versions - tutorial uses old version, checked 2024-02-18
    spdlog::info("Creating swapchain device...");

    wgpu::SwapChainDescriptor swapChainDesc{};
    swapChainDesc.nextInChain = nullptr;
    swapChainDesc.width = kWindowWidth;
    swapChainDesc.height = kWindowHeight;

    swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
    swapChainDesc.presentMode = WGPUPresentMode_Fifo;

#ifdef WGPU_BACKEND_WGPU
    wgpu::TextureFormat swapChainFormat{
        wgpuSurfaceGetPreferredFormat(surface, adapter)};
#else
    wgpu::TextureFormat swapChainFormat{WGPUTextureFormat_BGRA8Unorm};
#endif
    swapChainDesc.format = swapChainFormat;

    wgpu::SwapChain swapChain{
        wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc)};
    spdlog::info("SwapChain: {}\n", (void *)swapChain);

    auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void *
                              /* pUserData */) {
        spdlog::info("Queued work finished with status: {}\n", status);
    };

    wgpuQueueOnSubmittedWorkDone(queue,
                                 onQueueWorkDone,
                                 nullptr /*pUserData */);

    while (glfwWindowShouldClose(window) == 0)
    {
        glfwPollEvents();

        wgpu::TextureView nextTexture{
            wgpuSwapChainGetCurrentTextureView(swapChain)};
        if (nextTexture == nullptr)
        {
            spdlog::error("Cannot acquire next swap chain texture\n");
            break;
        }
        spdlog::info("nextTexture: {}", (void *)nextTexture);

        wgpu::CommandEncoderDescriptor commandEncoderDesc{};
        commandEncoderDesc.nextInChain = nullptr;
        commandEncoderDesc.label = "Command Encoder";
        wgpu::CommandEncoder encoder{
            wgpuDeviceCreateCommandEncoder(device, &commandEncoderDesc)};

        wgpu::RenderPassDescriptor renderPassDesc{};

        wgpu::RenderPassColorAttachment renderPassColorAttachment{};
        renderPassColorAttachment.view = nextTexture;
        renderPassColorAttachment.resolveTarget = nullptr;
        renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
        renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
        renderPassColorAttachment.clearValue = wgpu::Color{0.9, 0.1, 0.2, 1.0};
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &renderPassColorAttachment;


        renderPassDesc.depthStencilAttachment = nullptr;

        renderPassDesc.timestampWriteCount = 0;
        renderPassDesc.timestampWrites = nullptr;

        renderPassDesc.nextInChain = nullptr;

        wgpu::RenderPassEncoder renderPass{
            wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc)};
        wgpuRenderPassEncoderEnd(renderPass);

        wgpuTextureViewRelease(nextTexture);

        wgpu::CommandBufferDescriptor cmdBufferDescriptor{};
        cmdBufferDescriptor.nextInChain = nullptr;
        cmdBufferDescriptor.label = "Command buffer";
        WGPUCommandBuffer command{
            wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor)};
        wgpuCommandEncoderRelease(encoder);
        wgpuQueueSubmit(queue, 1, &command);
        wgpuCommandBufferRelease(command);

        wgpuSwapChainPresent(swapChain);
    }

    wgpuSwapChainRelease(swapChain);
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);
    wgpuSurfaceRelease(surface);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
