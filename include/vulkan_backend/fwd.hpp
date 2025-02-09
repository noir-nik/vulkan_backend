#pragma once

#include "vulkan_backend/config.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
// class PhysicalDevice;
class PhysicalDevice;
class Instance;
// class Instance;
class Device;
class Descriptor;
class BindlessDescriptor;
class Swapchain;
class Buffer;
class Image;
class Pipeline;
class Command;
class Queue;
class PipelineLibrary;

struct BufferInfo;
struct ImageInfo;
struct BindlessImageInfo;
struct BindlessbufferInfo;
struct PipelineInfo;
struct SubmitInfo;
struct RenderingInfo;
struct BlitInfo;
struct QueueInfo;
struct InstanceInfo;

} // namespace VB_NAMESPACE
