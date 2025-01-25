#pragma once

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/queue/info.hpp"
#include "vulkan_backend/types.hpp"


VB_EXPORT
namespace VB_NAMESPACE {
namespace defaults {
// By default device is created with one graphics queue
// This is extracted to static member, because
// libc++ on linux does not support P1394R4, so we can not use
// std::span range constructor
// constexpr inline vk::DeviceQueueCreateInfo kOneGraphicsQueue[] = {{vk::QueueFlagBits::eGraphics}};
constexpr inline QueueInfo kOneGraphicsQueue[] = {{vk::QueueFlagBits::eGraphics}};
constexpr inline QueueInfo kOneComputeQueue[]  = {{vk::QueueFlagBits::eCompute}};
// One Queue with graphics and compute capabilities
constexpr inline QueueInfo kOneGraphicsComputeQueue[] = {
	{vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute},
};
// One graphics and one separate compute queue
constexpr inline QueueInfo kOneGraphicsOneDedicatedComputeQueue[] = {
	{.flags = vk::QueueFlagBits::eGraphics},
	{.flags = vk::QueueFlagBits::eCompute, .undesired_flags = QueueInfo::kUndesiredFlagsForDedicatedCompute},
};

// Default staging buffer size
constexpr inline u32 kStagingSize = 64 * 1024 * 1024;

// Graphics pipeline library is preffered by default
constexpr inline const char* const kOptionalDeviceExtensions[] = {
	vk::EXTGraphicsPipelineLibraryExtensionName,
};
} // namespace defaults
} // namespace VB_NAMESPACE
