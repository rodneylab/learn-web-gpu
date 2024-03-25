#include "webgpu-utils.h"

#include <spdlog/spdlog.h>
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
        UserData &userData = *static_cast<UserData *>(pUserData);
        if (status == WGPURequestAdapterStatus_Success)
        {
            userData.adapter = adapter;
        }
        else
        {
            spdlog::error("Could not get WebGPU adapter: {}", message);
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

WGPUDevice requestDevice(WGPUAdapter adapter,
                         WGPUDeviceDescriptor const *descriptor)
{
    struct UserData
    {
        WGPUDevice device = nullptr;
        bool requestEnded = false;
    };

    UserData userData;

    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status,
                                   WGPUDevice device,
                                   char const *message,
                                   void *pUserData) {
        UserData &userData{*reinterpret_cast<UserData *>(pUserData)};
        if (status == WGPURequestDeviceStatus_Success)
        {
            userData.device = device;
        }
        else
        {
            spdlog::error("Could not get WebGPU device: {}", message);
        }
        userData.requestEnded = true;
    };

    wgpuAdapterRequestDevice(adapter,
                             descriptor,
                             onDeviceRequestEnded,
                             (void *)&userData);

    assert(userData.requestEnded);

    return userData.device;
}

void inspectAdapter(WGPUAdapter adapter)
{
    std::vector<WGPUFeatureName> features{};
    const size_t featureCount{wgpuAdapterEnumerateFeatures(adapter, nullptr)};
    features.resize(featureCount);
    wgpuAdapterEnumerateFeatures(adapter, features.data());

    spdlog::info("Adapter features:");
    for (auto feature : features)
    {
        spdlog::info(" — {}", feature);
    }

    WGPUSupportedLimits limits{};
    limits.nextInChain = nullptr;
    bool success{static_cast<bool>(wgpuAdapterGetLimits(adapter, &limits))};
    if (success)
    {
        spdlog::info("Adapter limits:");
        spdlog::info(" — maxTextureDimension1D: {}",
                     limits.limits.maxTextureDimension1D);
        spdlog::info(" — maxTextureDimension2D: {}",
                     limits.limits.maxTextureDimension2D);
        spdlog::info(" — maxTextureDimension3D: {}",
                     limits.limits.maxTextureDimension3D);
        spdlog::info(" — maxTextureArrayLayers: {}",
                     limits.limits.maxTextureArrayLayers);
        spdlog::info(" — maxBindGroups: {}", limits.limits.maxBindGroups);
        spdlog::info(" — maxDynamicUniformBuffersPerPipelineLayout: {}",
                     limits.limits.maxDynamicUniformBuffersPerPipelineLayout);
        spdlog::info(" — maxDynamicStorageBuffersPerPipelineLayout: {}",
                     limits.limits.maxDynamicStorageBuffersPerPipelineLayout);
        spdlog::info(" — maxSampledTexturesPerShaderStage: {}",
                     limits.limits.maxSampledTexturesPerShaderStage);
        spdlog::info(" — maxSamplersPerShaderStage: {}",
                     limits.limits.maxSampledTexturesPerShaderStage);
        spdlog::info(" — maxSamplersPerShaderStage: {}",
                     limits.limits.maxSamplersPerShaderStage);
        spdlog::info(" — maxStorageTexturesPerShaderStage: {}",
                     limits.limits.maxStorageTexturesPerShaderStage);
        spdlog::info(" — maxUniformBuffersPerShaderStage: {}",
                     limits.limits.maxUniformBuffersPerShaderStage);
        spdlog::info(" — maxUniformBufferBindingSize: {}",
                     limits.limits.maxUniformBufferBindingSize);
        spdlog::info(" — maxStorageBufferBindingSize: {}",
                     limits.limits.maxStorageBufferBindingSize);
        spdlog::info(" — minUniformBufferOffsetAlignment: {}",
                     limits.limits.minUniformBufferOffsetAlignment);
        spdlog::info(" — minStorageBufferOffsetAlignment: {}",
                     limits.limits.minStorageBufferOffsetAlignment);
        spdlog::info(" — maxVertexBuffers: {}", limits.limits.maxVertexBuffers);
        spdlog::info(" — maxVertexAttributes: {}",
                     limits.limits.maxVertexAttributes);
        spdlog::info(" — maxVertexBufferArrayStride: {}",
                     limits.limits.maxVertexBufferArrayStride);
        spdlog::info(" — maxInterStageShaderComponents: {}",
                     limits.limits.maxInterStageShaderComponents);
        spdlog::info(" — maxComputeWorkgroupStorageSize: {}",
                     limits.limits.maxComputeWorkgroupStorageSize);
        spdlog::info(" — maxComputeInvocationsPerWorkgroup: {}",
                     limits.limits.maxComputeInvocationsPerWorkgroup);
        spdlog::info(" — maxComputeWorkgroupSizeX: {}",
                     limits.limits.maxComputeWorkgroupSizeX);
        spdlog::info(" — maxComputeWorkgroupSizeY: {}",
                     limits.limits.maxComputeWorkgroupSizeY);
        spdlog::info(" — maxComputeWorkgroupSizeZ: {}",
                     limits.limits.maxComputeWorkgroupSizeZ);
        spdlog::info(" — maxComputeWorkgroupsPerDimension: {}",
                     limits.limits.maxComputeWorkgroupsPerDimension);
    }

    WGPUAdapterProperties properties{};
    properties.nextInChain = nullptr;
    wgpuAdapterGetProperties(adapter, &properties);
    spdlog::info("Adapter properties:");
    spdlog::info(" — vendorID: {}", properties.vendorID);
    spdlog::info(" — deviceID: {}", properties.deviceID);
    spdlog::info(" — name: {}", properties.name);
    if (strlen(properties.driverDescription))
    {
        spdlog::info(" — driverDescription: {}", properties.driverDescription);
    }
    spdlog::info(" — adapterType: {}", properties.adapterType);
    spdlog::info(" — backendType: {}", properties.backendType);
}

void inspectDevice(WGPUDevice device)
{
    std::vector<WGPUFeatureName> features{};
    const size_t featureCount{wgpuDeviceEnumerateFeatures(device, nullptr)};
    features.resize(featureCount);
    wgpuDeviceEnumerateFeatures(device, features.data());

    spdlog::info("Device features:");
    for (auto feature : features)
    {
        spdlog::info(" — {}\n", feature);
    }

    WGPUSupportedLimits limits{};
    limits.nextInChain = nullptr;
    bool success{static_cast<bool>(wgpuDeviceGetLimits(device, &limits))};
    if (success)
    {
        spdlog::info("Adapter limits:");
        spdlog::info(" — maxTextureDimension1D: {}",
                     limits.limits.maxTextureDimension1D);
        spdlog::info(" — maxTextureDimension2D: {}",
                     limits.limits.maxTextureDimension2D);
        spdlog::info(" — maxTextureDimension3D: {}",
                     limits.limits.maxTextureDimension3D);
        spdlog::info(" — maxTextureArrayLayers: {}",
                     limits.limits.maxTextureArrayLayers);
        spdlog::info(" — maxBindGroups: {}", limits.limits.maxBindGroups);
        spdlog::info(" — maxDynamicUniformBuffersPerPipelineLayout: {}",
                     limits.limits.maxDynamicUniformBuffersPerPipelineLayout);
        spdlog::info(" — maxDynamicStorageBuffersPerPipelineLayout: {}",
                     limits.limits.maxDynamicStorageBuffersPerPipelineLayout);
        spdlog::info(" — maxSampledTexturesPerShaderStage: {}",
                     limits.limits.maxSampledTexturesPerShaderStage);
        spdlog::info(" — maxSamplersPerShaderStage: {}",
                     limits.limits.maxSampledTexturesPerShaderStage);
        spdlog::info(" — maxSamplersPerShaderStage: {}",
                     limits.limits.maxSamplersPerShaderStage);
        spdlog::info(" — maxStorageTexturesPerShaderStage: {}",
                     limits.limits.maxStorageTexturesPerShaderStage);
        spdlog::info(" — maxUniformBuffersPerShaderStage: {}",
                     limits.limits.maxUniformBuffersPerShaderStage);
        spdlog::info(" — maxUniformBufferBindingSize: {}",
                     limits.limits.maxUniformBufferBindingSize);
        spdlog::info(" — maxStorageBufferBindingSize: {}",
                     limits.limits.maxStorageBufferBindingSize);
        spdlog::info(" — minUniformBufferOffsetAlignment: {}",
                     limits.limits.minUniformBufferOffsetAlignment);
        spdlog::info(" — minStorageBufferOffsetAlignment: {}",
                     limits.limits.minStorageBufferOffsetAlignment);
        spdlog::info(" — maxVertexBuffers: {}", limits.limits.maxVertexBuffers);
        spdlog::info(" — maxVertexAttributes: {}",
                     limits.limits.maxVertexAttributes);
        spdlog::info(" — maxVertexBufferArrayStride: {}",
                     limits.limits.maxVertexBufferArrayStride);
        spdlog::info(" — maxInterStageShaderComponents: {}",
                     limits.limits.maxInterStageShaderComponents);
        spdlog::info(" — maxComputeWorkgroupStorageSize: {}",
                     limits.limits.maxComputeWorkgroupStorageSize);
        spdlog::info(" — maxComputeInvocationsPerWorkgroup: {}",
                     limits.limits.maxComputeInvocationsPerWorkgroup);
        spdlog::info(" — maxComputeWorkgroupSizeX: {}",
                     limits.limits.maxComputeWorkgroupSizeX);
        spdlog::info(" — maxComputeWorkgroupSizeY: {}",
                     limits.limits.maxComputeWorkgroupSizeY);
        spdlog::info(" — maxComputeWorkgroupSizeZ: {}",
                     limits.limits.maxComputeWorkgroupSizeZ);
        spdlog::info(" — maxComputeWorkgroupsPerDimension: {}",
                     limits.limits.maxComputeWorkgroupsPerDimension);
    }
}
