#ifndef VULKAN_BACKEND_FWD_HPP_
#define VULKAN_BACKEND_FWD_HPP_

#ifndef VB_USE_STD_MODULE
#include <cstdint>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include <vulkan_backend/config.hpp>

VB_EXPORT
namespace VB_NAMESPACE {
using i32 = std::int32_t;
using u8  = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using vk::Flags;
using vk::BufferUsageFlags;
using vk::ImageUsageFlags;
using vk::ImageAspectFlags;
using vk::QueueFlags;
using vk::CullModeFlags;
using vk::ResolveModeFlags;
using vk::DeviceAddress;
using vk::Format;
using vk::Semaphore;
using vk::SemaphoreSubmitInfo;
using vk::CommandBufferSubmitInfo;
using vk::Fence;
using vk::Result;

using vk::Extent2D;
// using vk::Extent3D;
using vk::Offset2D;
using vk::Offset3D;
using vk::Rect2D;

using vk::ClearColorValue;
using vk::ClearDepthStencilValue;
using vk::ClearValue;
using vk::QueueFamilyIgnored;
using vk::DeviceSize;
using vk::WholeSize;
using vk::ImageLayout;
using vk::Filter;
using vk::CompareOp;
using vk::BorderColor;

using vk::operator&;
using vk::operator|;
using vk::operator^;
using vk::operator~;

using BufferUsage        = vk::BufferUsageFlagBits;
using ImageUsage         = vk::ImageUsageFlagBits;
using Aspect             = vk::ImageAspectFlagBits;
using ShaderStage        = vk::ShaderStageFlagBits;
using ResolveMode        = vk::ResolveModeFlagBits;
using SampleCount        = vk::SampleCountFlagBits;
using LoadOp             = vk::AttachmentLoadOp;
using StoreOp            = vk::AttachmentStoreOp;
using DynamicState       = vk::DynamicState;
using CullMode           = vk::CullModeFlagBits;
using DescriptorType     = vk::DescriptorType;
using QueueFlagBits      = vk::QueueFlagBits;
using PipelinePoint      = vk::PipelineBindPoint;
using PresentMode        = vk::PresentModeKHR;
using ColorSpace         = vk::ColorSpaceKHR;
using Surface            = vk::SurfaceKHR;
using SurfaceFormat      = vk::SurfaceFormatKHR;

using PipelineStageFlags = vk::PipelineStageFlags2;
using AccessFlags        = vk::AccessFlags2;
using PipelineStage      = vk::PipelineStageFlagBits2;
using Access             = vk::AccessFlagBits2;
using MipmapMode         = vk::SamplerMipmapMode;
using WrapMode           = vk::SamplerAddressMode;

struct PhysicalDeviceResource;
struct InstanceResource;
struct DeviceResource;
struct DescriptorResource;
struct SwapChainResource;
struct BufferResource;
struct ImageResource;
struct PipelineResource;
struct CommandResource;
struct QueueResource;
struct PipelineLibrary;

struct BufferInfo;
struct ImageInfo;
struct PipelineInfo;
struct SubmitInfo;
struct RenderingInfo;
struct BlitInfo;
struct QueueInfo;
struct InstanceInfo;

class Buffer;
class Command;
class Instance;
class Device;
class Image;
class Swapchain;
class Queue;
class Pipeline;
class Descriptor;

} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_FWD_HPP_