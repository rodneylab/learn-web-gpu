#ifndef SRC_WEBGPU_UTILS_H
#define SRC_WEBGPU_UTILS_H

#include <webgpu/webgpu.h>

WGPUAdapter requestAdapter(WGPUInstance instance,
                           WGPURequestAdapterOptions const *options);

WGPUDevice requestDevice(WGPUAdapter adapter,
                         WGPUDeviceDescriptor const *descriptor);

void inspectAdapter(WGPUAdapter adapter);

void inspectDevice(WGPUDevice device);

#endif
