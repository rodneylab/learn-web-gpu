#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <spdlog/spdlog.h>

#define WEBGPU_CPP_IMPLEMENTATION

#include <webgpu/webgpu.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <array>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

auto format_as(WGPUErrorType error_type)
{
    return fmt::underlying(error_type);
}

auto format_as(WGPUDeviceLostReason reason)
{
    return fmt::underlying(reason);
}

auto format_as(WGPUQueueWorkDoneStatus status)
{
    return fmt::underlying(status);
}

class Application
{
public:
    // Initialise everything and return true if it all went well
    bool Initialise();

    // Free everything that was initialised
    void Terminate();

    // Draw a frame and handle events
    void MainLoop();

    // Return true while we require the main loop to remain running
    bool IsRunning();

private:
    wgpu::TextureView GetNextSurfaceTextureView();

    GLFWwindow *window;
    std::optional<wgpu::Device> device{std::nullopt};
    std::optional<wgpu::Queue> queue{std::nullopt};
    std::optional<wgpu::Surface> surface{std::nullopt};
    std::unique_ptr<wgpu::ErrorCallback> uncapturedErrorCallbackHandle;
};

int main()
{
    Application app;

    if (!app.Initialise())
    {
        spdlog::error("Could not initialise WGPU!");
        return 1;
    }

#ifdef __EMSCRIPTEN__
    // Equivalent of the main loop when using Emscripten
    auto callback = [](void *arg) {
        Application *pApp = reinterpret_cast<Application *>(arg);
        pApp->MainLoop();
    };
    emscripten_set_main_loop_arg(callback, &app, 0, true);
#else  // __EMSCRIPTEN__
    while (app.IsRunning())
    {
        app.MainLoop();
    }
#endif // __EMSCRIPTEN__

    return 0;
}

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

bool Application::Initialise()
{
    // Open window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    constexpr int kWindowWidth{640};
    constexpr int kWindowHeight{480};
    window = glfwCreateWindow(kWindowWidth,
                              kWindowHeight,
                              "Learn WebGPU",
                              nullptr,
                              nullptr);

    wgpu::Instance instance{wgpuCreateInstance(nullptr)};

    surface = glfwGetWGPUSurface(instance, window);

    spdlog::info("Requesting adapter...");
    surface = glfwGetWGPUSurface(instance, window);
    wgpu::RequestAdapterOptions adapterOpts = {};
    adapterOpts.compatibleSurface = surface.value();
    wgpu::Adapter adapter{instance.requestAdapter(adapterOpts)};
    spdlog::info("Got adapter: {}", (void *)adapter);

    instance.release();

    spdlog::info("Requesting device...");
    wgpu::DeviceDescriptor deviceDesc = {};
    deviceDesc.label = "My Device";
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredLimits = nullptr;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The default queue";
    deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason,
                                       char const *message,
                                       void * /* pUserData */) {
        if (message)
        {
            spdlog::info("Device lost: reason: {} ({})", reason, message);
        }
        else
        {
            spdlog::info("Device lost: reason: {}", reason);
        }
    };
    device = std::optional<wgpu::Device>{adapter.requestDevice(deviceDesc)};
    spdlog::info("Got device: {}\n", (void *)device.value());

    uncapturedErrorCallbackHandle = device.value().setUncapturedErrorCallback(
        [](WGPUErrorType type, char const *message) {
            if (message)
            {
                spdlog::info("Uncaptured device error: type {} ({})",
                             type,
                             message);
            }
            else
            {
                spdlog::info("Uncaptured device error: type {}", type);
            }
        });

    queue = device.value().getQueue();

    // Configure the surface
    wgpu::SurfaceConfiguration config = {};
    config.width = kWindowWidth;
    config.height = kWindowHeight;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    wgpu::TextureFormat surfaceFormat =
        surface.value().getPreferredFormat(adapter);
    config.format = surfaceFormat;

    // we do not need any particular view format
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.device = device.value();
    config.presentMode = wgpu::PresentMode::Fifo;
    config.alphaMode = wgpu::CompositeAlphaMode::Auto;

    surface.value().configure(config);

    // Release the adapter only after we have fully initialised it
    adapter.release();

    glfwSetKeyCallback(window, key_callback);

    return true;
}

void Application::Terminate()
{
    if (surface.has_value())
    {
        surface.value().unconfigure();
    }
    if (queue.has_value())
    {
        queue.value().release();
    }
    if (surface.has_value())
    {
        surface.value().release();
    }
    if (device.has_value())
    {
        device.value().release();
    }
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Application::MainLoop()
{
    glfwPollEvents();

    // Get the next target texture view
    wgpu::TextureView targetView = GetNextSurfaceTextureView();
    if (!targetView)
    {
        return;
    }

    // Create a command encoder for the draw cell
    wgpu::CommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = "My command encoder";
    wgpu::CommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(device.value(), &encoderDesc);

    // Create the render pass that clears the screen with our colour
    wgpu::RenderPassDescriptor renderPassDesc = {};

    // The attachment part of the render pass descriptor describes the target texture of the pass
    wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = targetView;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
    renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
    renderPassColorAttachment.clearValue = WGPUColor{0.9, 0.1, 0.2, 1.0};
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.timestampWrites = nullptr;

    // Create the render pass and end it immediately )we only clear the screen, and do not draw anything)
    wgpu::RenderPassEncoder renderPass{encoder.beginRenderPass(renderPassDesc)};
    renderPass.end();
    renderPass.release();

    // Finally encode and submit the render pass
    wgpu::CommandBufferDescriptor cmdBufferDescriptor = {};
    cmdBufferDescriptor.label = "Command buffer";
    wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);
    encoder.release();

    spdlog::info("Submitting command...");
    queue.value().submit(1, &command);
    command.release();
    spdlog::info("Command submitted.");

    // At the end of the frame
    targetView.release();
#ifndef __EMSCRIPTEN__
    surface.value().present();
#endif

#if defined(WEBGPU_BACKEND_DAWN)
    wgpuDeviceTick(device.value());
#elif defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll(device.value(), false, nullptr);
#endif
}

bool Application::IsRunning()
{
    return !glfwWindowShouldClose(window);
}

wgpu::TextureView Application::GetNextSurfaceTextureView()
{
    // Get the surface texture
    wgpu::SurfaceTexture surfaceTexture;
    surface.value().getCurrentTexture(&surfaceTexture);
    if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::Success)
    {
        return nullptr;
    }
    wgpu::Texture texture{surfaceTexture.texture};

    // Create a view for this surface texture
    wgpu::TextureViewDescriptor viewDescriptor;
    viewDescriptor.label = "Surface texture view";
    viewDescriptor.format = texture.getFormat();
    viewDescriptor.dimension = wgpu::TextureViewDimension::_2D;
    viewDescriptor.baseMipLevel = 0;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.baseArrayLayer = 0;
    viewDescriptor.arrayLayerCount = 1;
    viewDescriptor.aspect = wgpu::TextureAspect::All;
    wgpu::TextureView targetView = texture.createView(viewDescriptor);

    return targetView;
}
