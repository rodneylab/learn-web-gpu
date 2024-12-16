#include "debug_assert.h"

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

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

namespace constants
{
inline constexpr int kWindowWidth{640};
inline constexpr int kWindowHeight{480};
} // namespace constants

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
fn vs_main(@location(0) in_vertex_position: vec2f) -> @builtin(position) vec4f {
    return vec4f(in_vertex_position, 0.0, 1.0);
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
    std::optional<wgpu::TextureView> GetNextSurfaceTextureView();

    // Substep of Initialise() that creates the render pipeline
    void InitialisePipeline();
    [[nodiscard]] static wgpu::RequiredLimits GetRequiredLimits(
        wgpu::Adapter adapter);
    void InitialiseBuffers();

    GLFWwindow *window{nullptr};
    std::optional<wgpu::Device> device{std::nullopt};
    std::optional<wgpu::Queue> queue{std::nullopt};
    std::optional<wgpu::Surface> surface{std::nullopt};
    std::unique_ptr<wgpu::ErrorCallback> uncapturedErrorCallbackHandle{nullptr};
    wgpu::TextureFormat surface_format{wgpu::TextureFormat::Undefined};
    std::optional<wgpu::RenderPipeline> pipeline{std::nullopt};
    std::optional<wgpu::Buffer> vertex_buffer{std::nullopt};
    uint32_t vertex_count{};
};

void signal_handler(int signal)
{
    if (signal == SIGABRT)
    {
        spdlog::info("Abort signal received");
        spdlog::error("Abort signal received");
        std::cerr << "Abort signal received\n";
    }
    else
    {
        spdlog::info("Unexpected signal received");
        spdlog::error("Unexpected signal received");
        std::cerr << "Unexpected signal received\n";
    }
    std::_Exit(EXIT_FAILURE);
}

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
    window = glfwCreateWindow(constants::kWindowWidth,
                              constants::kWindowHeight,
                              "Learn WebGPU",
                              nullptr,
                              nullptr);

    wgpu::Instance instance{wgpuCreateInstance(nullptr)};

    spdlog::info("Requesting adapter...");
    surface = glfwGetWGPUSurface(instance, window);
    wgpu::RequestAdapterOptions adapterOpts{};
    adapterOpts.compatibleSurface = surface.value();
    wgpu::Adapter adapter{instance.requestAdapter(adapterOpts)};
    spdlog::info("Got adapter: {}", (void *)adapter);

    wgpu::SupportedLimits supported_limits;
    adapter.getLimits(&supported_limits);

    spdlog::info("adapter.maxVertexAttributes: {}",
                 supported_limits.limits.maxVertexAttributes);

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
    wgpu::RequiredLimits required_limits{GetRequiredLimits(adapter)};
    deviceDesc.requiredLimits = &required_limits;
    device = std::optional<wgpu::Device>{adapter.requestDevice(deviceDesc)};
    spdlog::info("Got device: {}\n", (void *)device.value());

    device.value().getLimits(&supported_limits);
    spdlog::info("device.maxVertexAttributes: {}",
                 supported_limits.limits.maxVertexAttributes);

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
    config.width = constants::kWindowWidth;
    config.height = constants::kWindowHeight;
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
    InitialiseBuffers();

    return true;
}

void Application::Terminate()
{
    if (vertex_buffer.has_value())
    {
        vertex_buffer.value().release();
    }
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

    debug_assert(vertex_buffer.has_value(),
                 std::runtime_error(
                     fmt::format("Vertex buffer should be initialised before "
                                 "entering the main loop: [{}:{}]",
                                 __FILE__,
                                 __LINE__)));
    renderPass.setVertexBuffer(
        0,
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        vertex_buffer.value(),
        0,
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        vertex_buffer.value().getSize());
    renderPass.draw(vertex_count, 1, 0, 0);

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

    wgpu::VertexBufferLayout vertex_buffer_layout;
    wgpu::VertexAttribute position_attribute;
    position_attribute.shaderLocation = 0;
    position_attribute.format = wgpu::VertexFormat::Float32x2;
    position_attribute.offset = 0;

    vertex_buffer_layout.attributeCount = 1;
    vertex_buffer_layout.attributes = &position_attribute;

    vertex_buffer_layout.arrayStride = 2 * sizeof(float);
    vertex_buffer_layout.stepMode = wgpu::VertexStepMode::Vertex;

    pipeline_descriptor.vertex.bufferCount = 1;
    pipeline_descriptor.vertex.buffers = &vertex_buffer_layout;

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

wgpu::RequiredLimits Application::GetRequiredLimits(wgpu::Adapter adapter)
{
    wgpu::SupportedLimits supported_limits;
    adapter.getLimits(&supported_limits);

    wgpu::RequiredLimits required_limits{wgpu::Default};

    required_limits.limits.maxVertexAttributes = 1;
    required_limits.limits.maxVertexBuffers = 1;
    required_limits.limits.maxBufferSize =
        static_cast<long>(6) * 2 * sizeof(float);
    required_limits.limits.maxVertexBufferArrayStride = 2 * sizeof(float);

    // Default values might not be supported by the adapter, so assign adapter
    // the known supported minimum values
    required_limits.limits.minUniformBufferOffsetAlignment =
        supported_limits.limits.minUniformBufferOffsetAlignment;
    required_limits.limits.minStorageBufferOffsetAlignment =
        supported_limits.limits.minStorageBufferOffsetAlignment;

    // Value is being set to 0 by default, so when limits applied, all textures
    // are too big.
    // [Default from standard](https://www.w3.org/TR/webgpu/#limit-default]
    // is 8192.
    // https://www.w3.org/TR/webgpu/#limit-default default from standard
    constexpr int kDefaultMaxTextureDimension2d{8'192};
    required_limits.limits.maxTextureDimension2D =
        kDefaultMaxTextureDimension2d;

    return required_limits;
}

void Application::InitialiseBuffers()
{
    const std::vector<float> vertex_data{
        // clang-format off
        // NOLINTBEGIN(readability-magic-numbers)
        // each pair here forms the x,y coordinates of a  triangle vertex

        // first triangle
        -0.5F, -0.5F,
        0.5F, -0.5F,
        0.0F, 0.5F,

        // second triangle
        -0.55F, -0.5F,
        -0.05F, 0.5F,
        -0.55F, 0.5F,
        // NOLINTEND(readability-magic-numbers)
        // clang-format on
    };
    vertex_count = static_cast<uint32_t>(vertex_data.size() / 2);

    wgpu::BufferDescriptor buffer_descriptor{};
    buffer_descriptor.size = vertex_data.size() * sizeof(float);
    buffer_descriptor.usage =
        wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
    buffer_descriptor.mappedAtCreation = 0U;
    buffer_descriptor.label = "A couple of triangle x,y-vertex sets";
    debug_assert(device.has_value(),
                 std::runtime_error(
                     fmt::format("Device should be initialised before calling "
                                 "the InitialiseBuffers function: [{}:{}]",
                                 __FILE__,
                                 __LINE__)));
    vertex_buffer = std::optional<wgpu::Buffer>{
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        device.value().createBuffer(buffer_descriptor)};

    debug_assert(queue.has_value(),
                 std::runtime_error(
                     fmt::format("Queue should be initialised before calling "
                                 "the InitialiseBuffers function: [{}:{}]",
                                 __FILE__,
                                 __LINE__)));
    debug_assert(vertex_buffer.has_value(),
                 std::runtime_error(
                     fmt::format("Vertex buffer should have been initialised "
                                 "before attempting to write to it in "
                                 "the InitialiseBuffers function: [{}:{}]",
                                 __FILE__,
                                 __LINE__)));
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    queue.value().writeBuffer(vertex_buffer.value(),
                              0,
                              vertex_data.data(),
                              buffer_descriptor.size);
}
