#include "webgpu-utils.h"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <spdlog/spdlog.h>

#define WEBGPU_CPP_IMPLEMENTATION

#include <webgpu/webgpu.hpp>

#include <iostream>

static void key_callback(GLFWwindow *window,
                         int key,
                         int /* scancode */,
                         int action,
                         int /* mods */)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
}

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

    swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
    swapChainDesc.presentMode = wgpu::PresentMode::Fifo;

#ifdef WGPU_BACKEND_WGPU
    wgpu::TextureFormat swapChainFormat{
        wgpuSurfaceGetPreferredFormat(surface, adapter)};
#else
    // swapped value in tutorial - was producing darker hue
    // wgpu::TextureFormat swapChainFormat{WGPUTextureFormat_BGRA8Unorm};
    wgpu::TextureFormat swapChainFormat{wgpu::TextureFormat::BGRA8UnormSrgb};
#endif
    swapChainDesc.format = swapChainFormat;

    wgpu::SwapChain swapChain{
        wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc)};
    spdlog::info("SwapChain: {}", (void *)swapChain);

    spdlog::info("Creating shader module...");
    const char *shaderSource = R"(
@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4<f32> {
    var p = vec2f(0.0, 0.0);
    if in_vertex_index == 0u {
        p = vec2f(-0.5, -0.5);
    } else if in_vertex_index == 1u {
        p = vec2f(0.5, -0.5);
    } else {
        p = vec2f(0.0, 0.5);
    }
    return vec4f(p, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
    return vec4f(0.0, 0.4, 1.0, 1.0);
}
)";

    wgpu::ShaderModuleDescriptor shaderDesc;
#ifndef WEBGPU_BACKEND_WGPU
    shaderDesc.hintCount = 0;
    shaderDesc.hints = nullptr;
#endif

    wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc;
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    shaderDesc.nextInChain = &shaderCodeDesc.chain;

    shaderCodeDesc.code = shaderSource;

    wgpu::ShaderModule shaderModule{device.createShaderModule(shaderDesc)};
    spdlog::info("Shader module: {}", (void *)shaderModule);


    spdlog::info("Creating render pipeline...");
    wgpu::RenderPipelineDescriptor pipelineDesc;

    // Vertex fetch
    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;

    // Vertex shader
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;

    // Primitive assembly and rasterization
    // Each sequence of 3 vertices is considered as a triangle
    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    // When not specified, vertices are considered sequentially
    pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    // Corner vertices are enumerated in anti-clockwise order
    pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    // do not hide faces pointing away
    pipelineDesc.primitive.cullMode = wgpu::CullMode::None;

    // Fragment shader
    wgpu::FragmentState fragmentState;
    pipelineDesc.fragment = &fragmentState;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    // Configure blend state
    wgpu::BlendState blendState;
    blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = wgpu::BlendOperation::Add;
    blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blendState.alpha.dstFactor = wgpu::BlendFactor::One;
    blendState.alpha.operation = wgpu::BlendOperation::Add;

    wgpu::ColorTargetState colorTarget;
    colorTarget.format = swapChainFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    pipelineDesc.depthStencil = nullptr;

    // Multi-sampling
    // Samples per pixel
    pipelineDesc.multisample.count = 1;
    // Default value for the mae, meaning "all bits on"
    pipelineDesc.multisample.mask = ~0u;
    // Default value as well
    pipelineDesc.multisample.alphaToCoverageEnabled = false;


    pipelineDesc.layout = nullptr;

    wgpu::RenderPipeline pipeline = device.createRenderPipeline(pipelineDesc);
    spdlog::info("Render pipeline: {}", (void *)pipeline);


    auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void *
                              /* pUserData */) {
        spdlog::info("Queued work finished with status: {}\n", status);
    };

    wgpuQueueOnSubmittedWorkDone(queue,
                                 onQueueWorkDone,
                                 nullptr /*pUserData */);

    glfwSetKeyCallback(window, key_callback);

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
        renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
        renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
        renderPassColorAttachment.clearValue = wgpu::Color{0.9, 0.1, 0.2, 1.0};
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &renderPassColorAttachment;

        renderPassDesc.depthStencilAttachment = nullptr;
        renderPassDesc.timestampWriteCount = 0;
        renderPassDesc.timestampWrites = nullptr;
        wgpu::RenderPassEncoder renderPass{
            encoder.beginRenderPass(renderPassDesc)};

        renderPassDesc.nextInChain = nullptr;

        encoder.beginRenderPass(renderPassDesc);

        renderPass.setPipeline(pipeline);
        renderPass.draw(3, 1, 0, 0);
        renderPass.end();
        renderPass.release();

        nextTexture.release();

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
