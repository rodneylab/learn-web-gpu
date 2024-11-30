#include <GLFW/glfw3.h>
#include <fmt/format.h>
#include <glfw3webgpu.h>
#include <spdlog/spdlog.h>

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.h>
#include <webgpu/webgpu.hpp>
#include <webgpu/wgpu.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <memory>
#include <optional>

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

const char *const shader_source{R"(
@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
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
)"};

class Application
{
public:
    // Initialise everything and return true if it all went well
    bool Initialise();

    // Free everything that was initialised
    void Terminate();

    // Draw a frame and handle events
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    void MainLoop();

    // Return true while we require the main loop to remain running
    bool IsRunning();

private:
    // Substep of Initialise() that creates the render pipeline
    void InitialisePipeline();

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    std::optional<wgpu::TextureView> GetNextSurfaceTextureView();

    GLFWwindow *window{nullptr};
    std::optional<wgpu::Device> device{std::nullopt};
    std::optional<wgpu::Queue> queue{std::nullopt};
    std::optional<wgpu::Surface> surface{std::nullopt};
    std::unique_ptr<wgpu::ErrorCallback> uncapturedErrorCallbackHandle{nullptr};
    wgpu::TextureFormat surface_format{wgpu::TextureFormat::Undefined};
    std::optional<wgpu::RenderPipeline> pipeline{std::nullopt};
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

namespace
{
void key_callback(GLFWwindow *window,
                  int key,
                  int /* scancode */,
                  int action,
                  int mods)
{
    if (((key == GLFW_KEY_ESCAPE) ||
         ((key == GLFW_KEY_W || key == GLFW_KEY_Q) &&
          mods == GLFW_MOD_SUPER)) &&
        action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}
} // namespace

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
    // NOLINTNEXTLINE(misc-const-correctness)
    wgpu::RequestAdapterOptions adapterOpts{};
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
        if (message != nullptr)
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
        [](WGPUErrorType error_type, char const *message) {
            if (message != nullptr)
            {
                spdlog::info("Uncaptured device error: type {} ({})",
                             error_type,
                             message);
            }
            else
            {
                spdlog::info("Uncaptured device error: type {}", error_type);
            }
        });

    queue = device.value().getQueue();

    // Configure the surface
    wgpu::SurfaceConfiguration config = {};
    config.width = kWindowWidth;
    config.height = kWindowHeight;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    surface_format = surface.value().getPreferredFormat(adapter);
    config.format = surface_format;

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

    InitialisePipeline();

    return true;
}

void Application::Terminate()
{
    if (pipeline.has_value())
    {
        pipeline.value().release();
    }
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

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void Application::MainLoop()
{
    glfwPollEvents();

    // Get the next target texture view
    std::optional<wgpu::TextureView> target_view{GetNextSurfaceTextureView()};
    if (!target_view.has_value())
    {
        return;
    }

    // Create a command encoder for the draw cell
    wgpu::CommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = "My command encoder";
    if (!device.has_value())
    {
        spdlog::error(
            "Device should be initialised before entering the main loop");
        return;
    }
    wgpu::CommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(device.value(), &encoderDesc);

    // Create the render pass that clears the screen with our colour
    wgpu::RenderPassDescriptor renderPassDesc = {};

    // The attachment part of the render pass descriptor describes the target texture of the pass
    wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = target_view.value();
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
    renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;

    constexpr double kClearRedColour{0.9};
    constexpr double kClearGreenColour{0.1};
    constexpr double kClearBlueColour{0.2};
    renderPassColorAttachment.clearValue =
        wgpu::Color{kClearRedColour, kClearGreenColour, kClearBlueColour, 1.0};
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.timestampWrites = nullptr;

    // Create the render pass and end it immediately )we only clear the screen, and do not draw anything)
    wgpu::RenderPassEncoder renderPass{encoder.beginRenderPass(renderPassDesc)};
    if (pipeline.has_value() && pipeline.value() != nullptr)
    {
        renderPass.setPipeline(pipeline.value());
    }
    else
    {
        spdlog::error(
            "Pipeline should be initialised before entering main loop");
    }
    renderPass.draw(3, 1, 0, 0);

    renderPass.end();
    renderPass.release();

    // Finally encode and submit the render pass
    wgpu::CommandBufferDescriptor cmdBufferDescriptor = {};
    cmdBufferDescriptor.label = "Command buffer";
    wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);
    encoder.release();

    spdlog::info("Submitting command...");
    if (queue.has_value())
    {
        queue.value().submit(1, &command);
    }
    else
    {
        spdlog::error(
            "Queue should be initialised before entering the main loop");
    }
    command.release();
    spdlog::info("Command submitted.");

    // At the end of the frame
    target_view.value().release();
#ifndef __EMSCRIPTEN__
    if (surface.has_value())
    {
        surface.value().present();
    }
    else
    {
        spdlog::error(
            "Surface should be initialised before entering the main loop");
    }
#endif

    if (device.has_value())
    {
#if defined(WEBGPU_BACKEND_DAWN)
        // wgpuDeviceTick(device.value());
#elif defined(WEBGPU_BACKEND_WGPU)
        wgpuDevicePoll(device.value(), 0U, nullptr);
#endif
    }
    else
    {
        spdlog::error(
            "Device should be initialised before entering the main loop");
    }
}

bool Application::IsRunning()
{
    return glfwWindowShouldClose(window) == 0;
}

std::optional<wgpu::TextureView> Application::GetNextSurfaceTextureView()
{
    // Get the surface texture
    wgpu::SurfaceTexture surfaceTexture;
    if (surface.has_value())
    {
        surface.value().getCurrentTexture(&surfaceTexture);
    }
    else
    {
        spdlog::error("Surface should be initialise before getting the "
                      "current texture");
    }

    if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::Success)
    {
        return std::nullopt;
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
    const wgpu::TextureView targetView = texture.createView(viewDescriptor);

#ifndef WEBGPU_BACKEND_WGPU
    Texture(surfaceTexture.texture).release();
#endif

    return targetView;
}

void Application::InitialisePipeline()
{
    // Load the shader module
    wgpu::ShaderModuleDescriptor shader_descriptor{};
#ifdef WEBGPU_BACKEND_WGPU
    shader_descriptor.hintCount = 0;
    shader_descriptor.hints = nullptr;
#endif

    // Use the extension mechanism to specify the WGSL part of the shader module descriptor
    wgpu::ShaderModuleWGSLDescriptor shader_code_descriptor{};

    // Set the chained struct's header
    shader_code_descriptor.chain.next = nullptr;
    shader_code_descriptor.chain.sType =
        wgpu::SType::ShaderModuleWGSLDescriptor;

    // Connect the chain
    shader_descriptor.nextInChain = &shader_code_descriptor.chain;
    shader_code_descriptor.code = shader_source;
    if (!device.has_value())
    {
        spdlog::error(
            "Device should be initialised before creating a shader module");
        return;
    }
    wgpu::ShaderModule shader_module{
        device.value().createShaderModule(shader_descriptor)};

    // Create the render pipeline
    wgpu::RenderPipelineDescriptor pipeline_descriptor{};

    // We do not use any vertex buffer for this first example
    pipeline_descriptor.vertex.bufferCount = 0;
    pipeline_descriptor.vertex.buffers = nullptr;

    pipeline_descriptor.vertex.module = shader_module;
    pipeline_descriptor.vertex.entryPoint = "vs_main";
    pipeline_descriptor.vertex.constantCount = 0;
    pipeline_descriptor.vertex.constants = nullptr;

    // Each sequence of 3 vertices is considered as a triangle
    pipeline_descriptor.primitive.topology =
        wgpu::PrimitiveTopology::TriangleList;

    pipeline_descriptor.primitive.stripIndexFormat =
        wgpu::IndexFormat::Undefined;

    // Face orientation is defined by assuming that when looking from the front
    // of the face that its corner vertices are enumerated in anti-clockwise
    // (a.k.a counter-clockwise,  (**CCW**)) order.
    pipeline_descriptor.primitive.frontFace = wgpu::FrontFace::CCW;

    pipeline_descriptor.primitive.cullMode = wgpu::CullMode::None;

    wgpu::FragmentState fragment_state{};
    fragment_state.module = shader_module;
    fragment_state.entryPoint = "fs_main";
    fragment_state.constantCount = 0;
    fragment_state.constants = nullptr;

    wgpu::BlendState blend_state{};
    blend_state.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend_state.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend_state.color.operation = wgpu::BlendOperation::Add;
    blend_state.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blend_state.alpha.dstFactor = wgpu::BlendFactor::One;
    blend_state.alpha.operation = wgpu::BlendOperation::Add;

    wgpu::ColorTargetState colour_target{};
    colour_target.format = surface_format;
    colour_target.blend = &blend_state;
    colour_target.writeMask = wgpu::ColorWriteMask::All;

    fragment_state.targetCount = 1;
    fragment_state.targets = &colour_target;
    pipeline_descriptor.fragment = &fragment_state;

    pipeline_descriptor.depthStencil = nullptr;
    pipeline_descriptor.multisample.count = 1;
    pipeline_descriptor.multisample.mask = ~0U;

    pipeline_descriptor.multisample.alphaToCoverageEnabled = 0U;
    pipeline_descriptor.layout = nullptr;

    if (device.has_value())
    {
        pipeline = std::optional<wgpu::RenderPipeline>{
            device.value().createRenderPipeline(pipeline_descriptor)};
    }
    else
    {
        spdlog::error("Device should be initialised before the pipeline");
    }

    shader_module.release();
}
