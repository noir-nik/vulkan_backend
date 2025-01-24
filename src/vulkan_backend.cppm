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
export import vulkan_hpp;
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
/* 
"structs.hpp"
*/
using VB_NAMESPACE::Viewport;
using VB_NAMESPACE::Extent3D;
} // namespace VB_NAMESPACE
export {
using VB_NAMESPACE::operator&;
using VB_NAMESPACE::operator|;
using VB_NAMESPACE::operator^;
using VB_NAMESPACE::operator~;
}

// NOLINTEND(misc-unused-using-decls)
