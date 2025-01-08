module;

export module vulkan_backend;

#ifndef VB_USE_STD_MODULE
#define VB_USE_STD_MODULE 0
#endif

#ifndef VB_USE_VULKAN_MODULE
#define VB_USE_VULKAN_MODULE 0
#endif

#if VB_USE_STD_MODULE
import std;
#endif

#if VB_USE_VULKAN_MODULE
import vulkan_hpp;
#endif

#ifdef VB_DEV
#undef VB_DEV
#endif

#define VB_EXPORT export
extern "C++" {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winclude-angled-in-module-purview"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 5244)
#endif

#if VB_USE_STD_MODULE
#include <cassert>
#endif

#include "vulkan_backend/core.hpp"

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
}

// This is optional, if extern "C++" method causes ODR violations
// NOLINTBEGIN(misc-unused-using-decls)
export namespace VB_NAMESPACE {
/* 
	"core.hpp"
*/
// Structs
using VB_NAMESPACE::BufferInfo;
using VB_NAMESPACE::ImageInfo;
using VB_NAMESPACE::Pipeline;
using VB_NAMESPACE::PipelineInfo;
using VB_NAMESPACE::SubmitInfo;
using VB_NAMESPACE::RenderingInfo;
using VB_NAMESPACE::BlitInfo;
using VB_NAMESPACE::InstanceInfo;
using VB_NAMESPACE::DeviceInfo;

// Handles
using VB_NAMESPACE::Buffer;
using VB_NAMESPACE::Image;
using VB_NAMESPACE::Queue;
using VB_NAMESPACE::Command;
using VB_NAMESPACE::Device;
using VB_NAMESPACE::Swapchain;
using VB_NAMESPACE::PhysicalDevice;
using VB_NAMESPACE::Instance;

// Enums
using VB_NAMESPACE::LogLevel;

// functions
using VB_NAMESPACE::CreateInstance;
using VB_NAMESPACE::StringFromVkResult;
using VB_NAMESPACE::CheckVkResultDefault;

using VB_NAMESPACE::Memory;
using VB_NAMESPACE::BufferUsage;
using VB_NAMESPACE::ImageUsageFlags;
using VB_NAMESPACE::Aspect;
using VB_NAMESPACE::QueueFlagBits;
using VB_NAMESPACE::CullMode;
using VB_NAMESPACE::PipelineStage;
using VB_NAMESPACE::Access;
using VB_NAMESPACE::ResolveMode;

using VB_NAMESPACE::MemoryFlags;
using VB_NAMESPACE::BufferUsageFlags;
using VB_NAMESPACE::Format;
using VB_NAMESPACE::ImageUsageFlags;
using VB_NAMESPACE::Aspect;
using VB_NAMESPACE::QueueFlags;
using VB_NAMESPACE::CullModeFlags;
using VB_NAMESPACE::PipelineStageFlags;
using VB_NAMESPACE::AccessFlags;
using VB_NAMESPACE::ResolveModeFlags;

using VB_NAMESPACE::ImageLayout;
using VB_NAMESPACE::SampleCount;
using VB_NAMESPACE::Filter;
using VB_NAMESPACE::MipmapMode;
using VB_NAMESPACE::WrapMode;
using VB_NAMESPACE::CompareOp;
using VB_NAMESPACE::BorderColor;
using VB_NAMESPACE::DescriptorType;
using VB_NAMESPACE::PipelinePoint;
using VB_NAMESPACE::ShaderStage;
using VB_NAMESPACE::DynamicState;
using VB_NAMESPACE::LoadOp;
using VB_NAMESPACE::StoreOp;
using VB_NAMESPACE::PresentMode;
using VB_NAMESPACE::ColorSpace;

/* 
"structs.hpp"
*/
using VB_NAMESPACE::Viewport;
using VB_NAMESPACE::Extent2D;
using VB_NAMESPACE::Extent3D;
using VB_NAMESPACE::Offset2D;
using VB_NAMESPACE::Offset3D;
using VB_NAMESPACE::Rect2D;
using VB_NAMESPACE::ClearColorValue;
using VB_NAMESPACE::ClearDepthStencilValue;
using VB_NAMESPACE::ClearValue;
using VB_NAMESPACE::ImageBlit;
using VB_NAMESPACE::SamplerInfo;
using VB_NAMESPACE::MemoryBarrier;
using VB_NAMESPACE::BufferBarrier;
using VB_NAMESPACE::ImageBarrier;
}

export {
using VB_NAMESPACE::operator&;
using VB_NAMESPACE::operator|;
using VB_NAMESPACE::operator^;
using VB_NAMESPACE::operator~;
}

// NOLINTEND(misc-unused-using-decls)
