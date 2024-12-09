#include "debug_assert.h"

#include <GLFW/glfw3.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <glfw3webgpu.h>
#include <spdlog/spdlog.h>

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.h>
#include <webgpu/webgpu.hpp>
#include <webgpu/wgpu.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <vector>

class Error
{
};

auto format_as(WGPUErrorType error_type)
{
    return fmt::underlying(error_type);
}

auto format_as(WGPUDeviceLostReason reason)
{
    return fmt::underlying(reason);
}

auto format_as(WGPUBufferMapAsyncStatus status)
{
    return fmt::underlying(status);
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

void wgpu_poll_events([[maybe_unused]] wgpu::Device device,
                      [[maybe_unused]] bool yield_to_browser)
{
#if defined(WEBGPU_BACKEND_DAWN)
    device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
    // device.poll(false);
    wgpuDevicePoll(device, 0U, nullptr);
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
    if (yield_to_browser)
    {
        emscripten_sleep(100);
    }
#endif
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
    // Substep of Initialise() that creates the render pipeline
    void InitialisePipeline();

    void PlayingWithBuffers();

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
    try
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
    catch (const std::exception &e)
    {
        spdlog::error("Error: {}", e.what());
    }
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

    PlayingWithBuffers();

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

void Application::MainLoop()
{
    glfwPollEvents();

    // Get the next target texture view
    std::optional<wgpu::TextureView> target_view{GetNextSurfaceTextureView()};
    debug_assert(target_view.has_value(),
                 std::runtime_error(
                     fmt::format("Target view should be initialised before "
                                 "entering the main loop: [{}:{}]",
                                 __FILE__,
                                 __LINE__)));
    if (!target_view.has_value())
    {
        return;
    }

    // Create a command encoder for the draw cell
    wgpu::CommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = "My command encoder";
    debug_assert(
        device.has_value(),
        std::runtime_error(fmt::format("Device should be initialised before "
                                       "entering the main loop: [{}:{}]",
                                       __FILE__,
                                       __LINE__)));
    wgpu::CommandEncoder encoder =
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
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
    debug_assert(
        queue.has_value(),
        std::runtime_error(fmt::format("Queue should be initialised before "
                                       "entering the main loop: [{}:{}]",
                                       __FILE__,
                                       __LINE__)));
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    queue.value().submit(1, &command);

    command.release();
    spdlog::info("Command submitted.");

    // At the end of the frame
    target_view.value().release();
#ifndef __EMSCRIPTEN__
    debug_assert(
        surface.has_value(),
        std::runtime_error(fmt::format("Surface should be initialised before "
                                       "entering the main loop: [{}:{}]",
                                       __FILE__,
                                       __LINE__)));
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    surface.value().present();
#endif

    debug_assert(
        device.has_value(),
        std::runtime_error(fmt::format("Device should be initialised before "
                                       "entering the main loop: [{}:{}]",
                                       __FILE__,
                                       __LINE__)));
#if defined(WEBGPU_BACKEND_DAWN)
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    wgpuDeviceTick(device.value());
#elif defined(WEBGPU_BACKEND_WGPU)
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    wgpuDevicePoll(device.value(), 0U, nullptr);
#endif
}

bool Application::IsRunning()
{
    return glfwWindowShouldClose(window) == 0;
}

std::optional<wgpu::TextureView> Application::GetNextSurfaceTextureView()
{
    // Get the surface texture
    wgpu::SurfaceTexture surfaceTexture;
    debug_assert(surface.has_value(),
                 std::runtime_error(fmt::format(
                     "Surface should be initialise before getting the "
                     "current texture: [{}:{}]",
                     __FILE__,
                     __LINE__)));
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    surface.value().getCurrentTexture(&surfaceTexture);

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
    debug_assert(
        device.has_value(),
        std::runtime_error(fmt::format("Device should be initialised before "
                                       "creating a shader module: [{}:{}]",
                                       __FILE__,
                                       __LINE__)));
    wgpu::ShaderModule shader_module{
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
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

    debug_assert(
        device.has_value(),
        std::runtime_error(fmt::format("Device should be initialised before "
                                       "the pipeline: [{}:{}]",
                                       __FILE__,
                                       __LINE__)));
    pipeline = std::optional<wgpu::RenderPipeline>{
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        device.value().createRenderPipeline(pipeline_descriptor)};

    shader_module.release();
}

void Application::PlayingWithBuffers()
{
    wgpu::BufferDescriptor buffer_descriptor{};
    buffer_descriptor.label = "Some GPU-side data buffer";
    buffer_descriptor.usage =
        wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc;
    constexpr int kBufferSize{16};
    buffer_descriptor.size = kBufferSize;
    buffer_descriptor.mappedAtCreation = 0U;
    debug_assert(device.has_value(),
                 std::runtime_error(
                     fmt::format("Device should be initialised before calling "
                                 "the PlayingWithBuffers function: [{}:{}]",
                                 __FILE__,
                                 __LINE__)));
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    wgpu::Buffer buffer_1{device.value().createBuffer(buffer_descriptor)};
    buffer_descriptor.label = "Output buffer";
    buffer_descriptor.usage =
        wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    wgpu::Buffer buffer_2{device.value().createBuffer(buffer_descriptor)};

    std::vector<uint8_t> numbers(kBufferSize);
    uint8_t number_value{0};
    for (auto &number : numbers)
    {
        number = number_value;
        ++number_value;
    }
    spdlog::info("buffer_data = [ {} ]", fmt::join(numbers, ", "));

    debug_assert(queue.has_value(),
                 std::runtime_error(
                     fmt::format("Queue should be initialised before calling "
                                 "the PlayingWithBuffers function: [{}:{}]",
                                 __FILE__,
                                 __LINE__)));
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    queue.value().writeBuffer(buffer_1, 0, numbers.data(), numbers.size());
    wgpu::CommandEncoder encoder{
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        device.value().createCommandEncoder(wgpu::Default)};

    encoder.copyBufferToBuffer(buffer_1, 0, buffer_2, 0, kBufferSize);

    wgpu::CommandBuffer command{encoder.finish(wgpu::Default)};
    encoder.release();
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    queue.value().submit(1, &command);
    command.release();

    struct Context
    {
        bool ready;
        wgpu::Buffer buffer;
    };

    WGPUBufferMapCallback on_buffer_to_mapped =
        [](WGPUBufferMapAsyncStatus status, void *user_data) {
            Context *context{static_cast<Context *>(user_data)};
            context->ready = true;
            spdlog::info("Buffer to mapped with status {}", status);
            if (status != wgpu::BufferMapAsyncStatus::Success)
            {
                return;
            }

            // Get a pointer to wherever the driver mapped the GPU memory to the RAM
            constexpr int kBufferSize{16};
            const auto *buffer_data_c_array = static_cast<const uint8_t *>(
                context->buffer.getConstMappedRange(0, kBufferSize));
            std::vector<uint8_t> buffer_data;
            buffer_data.reserve(kBufferSize);
            buffer_data.assign(*buffer_data_c_array,
                               *buffer_data_c_array + kBufferSize);
            spdlog::info("buffer_data = [ {} ]", fmt::join(buffer_data, ", "));
            context->buffer.unmap();
        };

    const Context context{false, buffer_2};
    wgpuBufferMapAsync(buffer_2,
                       wgpu::MapMode::Read,
                       0,
                       kBufferSize,
                       on_buffer_to_mapped,
                       (void *)&context);

    while (!context.ready)
    {
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        wgpu_poll_events(device.value(), true);
    }

    buffer_1.release();
    buffer_2.release();
}
