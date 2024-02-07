#include <GLFW/glfw3.h>
#include <fmt/core.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

#include <cassert>
#include <iostream>
#include <vector>

WGPUAdapter requestAdapter(WGPUInstance instance,
                           WGPURequestAdapterOptions const *options)
{
    struct UserData
    {
        WGPUAdapter adapter{nullptr};
        bool requestEnded{false};
    };
    UserData userData;

    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status,
                                    WGPUAdapter adapter,
                                    char const *message,
                                    void *pUserData) {
        UserData &userData = *reinterpret_cast<UserData *>(pUserData);
        if (status == WGPURequestAdapterStatus_Success)
        {
            userData.adapter = adapter;
        }
        else
        {
            fmt::print("[ ERROR ] Could not get WebGPU adapter: {}\n", message);
        }
        userData.requestEnded = true;
    };

    wgpuInstanceRequestAdapter(instance,
                               options,
                               onAdapterRequestEnded,
                               (void *)&userData);
    assert(userData.requestEnded);

    return userData.adapter;
}

void inspectAdapter(WGPUAdapter adapter)
{
    std::vector<WGPUFeatureName> features{};
    const size_t featureCount{wgpuAdapterEnumerateFeatures(adapter, nullptr)};
    features.resize(featureCount);
    wgpuAdapterEnumerateFeatures(adapter, features.data());

    std::cout << "Adapter features:\n";
    for (auto feature : features)
    {
        fmt::print("[ INFO ] — {}\n", (void *)feature);
    }

    WGPUSupportedLimits limits{};
    limits.nextInChain = nullptr;
    bool success{static_cast<bool>(wgpuAdapterGetLimits(adapter, &limits))};
    if (success)
    {
        std::cout << "[ INFO ] Adapter limits:\n";
        fmt::print("[ INFO ]  — maxTextureDimension1D: {}\n",
                   limits.limits.maxTextureDimension1D);
        fmt::print("[ INFO ]  — maxTextureDimension2D: {}\n",
                   limits.limits.maxTextureDimension2D);
        fmt::print("[ INFO ]  — maxTextureDimension3D: {}\n",
                   limits.limits.maxTextureDimension3D);
        fmt::print("[ INFO ]  — maxTextureArrayLayers: {}\n",
                   limits.limits.maxTextureArrayLayers);
        fmt::print("[ INFO ]  — maxBindGroups: {}\n",
                   limits.limits.maxBindGroups);
        fmt::print(
            "[ INFO ]  — maxDynamicUniformBuffersPerPipelineLayout: {}\n",
            limits.limits.maxDynamicUniformBuffersPerPipelineLayout);
        fmt::print(
            "[ INFO ]  — maxDynamicStorageBuffersPerPipelineLayout: {}\n",
            limits.limits.maxDynamicStorageBuffersPerPipelineLayout);
        fmt::print("[ INFO ]  — maxSampledTexturesPerShaderStage: {}\n",
                   limits.limits.maxSampledTexturesPerShaderStage);
        fmt::print("[ INFO ]  — maxSamplersPerShaderStage: {}\n",
                   limits.limits.maxSampledTexturesPerShaderStage);
        fmt::print("[ INFO ]  — maxSamplersPerShaderStage: {}\n",
                   limits.limits.maxSamplersPerShaderStage);
        fmt::print("[ INFO ]  — maxStorageTexturesPerShaderStage: {}\n",
                   limits.limits.maxStorageTexturesPerShaderStage);
        fmt::print("[ INFO ]  — maxUniformBuffersPerShaderStage: {}\n",
                   limits.limits.maxUniformBuffersPerShaderStage);
        fmt::print("[ INFO ]  — maxUniformBufferBindingSize: {}\n",
                   limits.limits.maxUniformBufferBindingSize);
        fmt::print("[ INFO ]  — maxStorageBufferBindingSize: {}\n",
                   limits.limits.maxStorageBufferBindingSize);
        fmt::print("[ INFO ]  — minUniformBufferOffsetAlignment: {}\n",
                   limits.limits.minUniformBufferOffsetAlignment);
        fmt::print("[ INFO ]  — minStorageBufferOffsetAlignment: {}\n",
                   limits.limits.minStorageBufferOffsetAlignment);
        fmt::print("[ INFO ]  — maxVertexBuffers: {}\n",
                   limits.limits.maxVertexBuffers);
        fmt::print("[ INFO ]  — maxVertexAttributes: {}\n",
                   limits.limits.maxVertexAttributes);
        fmt::print("[ INFO ]  — maxVertexBufferArrayStride: {}\n",
                   limits.limits.maxVertexBufferArrayStride);
        fmt::print("[ INFO ]  — maxInterStageShaderComponents: {}\n",
                   limits.limits.maxInterStageShaderComponents);
        fmt::print("[ INFO ]  — maxComputeWorkgroupStorageSize: {}\n",
                   limits.limits.maxComputeWorkgroupStorageSize);
        fmt::print("[ INFO ]  — maxComputeInvocationsPerWorkgroup: {}\n",
                   limits.limits.maxComputeInvocationsPerWorkgroup);
        fmt::print("[ INFO ]  — maxComputeWorkgroupSizeX: {}\n",
                   limits.limits.maxComputeWorkgroupSizeX);
        fmt::print("[ INFO ]  — maxComputeWorkgroupSizeY: {}\n",
                   limits.limits.maxComputeWorkgroupSizeY);
        fmt::print("[ INFO ]  — maxComputeWorkgroupSizeZ: {}\n",
                   limits.limits.maxComputeWorkgroupSizeZ);
        fmt::print("[ INFO ]  — maxComputeWorkgroupsPerDimension: {}\n",
                   limits.limits.maxComputeWorkgroupsPerDimension);
    }


    WGPUAdapterProperties properties{};
    properties.nextInChain = nullptr;
    wgpuAdapterGetProperties(adapter, &properties);
    std::cout << "[ INFO ] Adapter properties:\n";
    fmt::print("[ INFO ] — vendorID: {}\n", properties.vendorID);
    fmt::print("[ INFO ] — deviceID: {}\n", properties.deviceID);
    fmt::print("[ INFO ] — name: {}\n", properties.name);
    if (strlen(properties.driverDescription))
    {
        fmt::print("[ INFO ] — driverDescription: {}\n",
                   properties.driverDescription);
    }
    fmt::print("[ INFO ] — adapterType: {}\n", (void *)properties.adapterType);
    fmt::print("[ INFO ] — backendType: {}\n", (void *)properties.backendType);
}

int main(int /*unused*/, char ** /*unused*/)
{
    WGPUInstanceDescriptor desc = {};
    desc.nextInChain = nullptr;
    WGPUInstance instance{wgpuCreateInstance(&desc)};

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

    constexpr int kWINDOW_WIDTH{640};
    constexpr int kWINDOW_HEIGHT{480};
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

    std::cout << "Requesting adapter...\n";

    // Utility function provided by glfw3webgpu.h
    WGPUSurface surface = glfwGetWGPUSurface(instance, window);

    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    adapterOpts.compatibleSurface = surface;

    WGPUAdapter adapter = requestAdapter(instance, &adapterOpts);

    std::cout << "Got adapter: " << adapter << '\n';

    inspectAdapter(adapter);

    while (glfwWindowShouldClose(window) == 0)
    {
        glfwPollEvents();
    }

    wgpuSurfaceRelease(surface);
    wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
