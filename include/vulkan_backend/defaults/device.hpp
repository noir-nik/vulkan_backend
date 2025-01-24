#pragma once

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/info/queue.hpp"
#include "vulkan_backend/interface/physical_device.hpp"
#include "vulkan_backend/types.hpp"


VB_EXPORT
namespace VB_NAMESPACE {
namespace defaults {
// By default device is created with one graphics queue
// This is extracted to static member, because
// libc++ on linux does not support P1394R4, so we can not use
// std::span range constructor
constexpr inline QueueInfo kOneGraphicsQueue[] = {{vk::QueueFlagBits::eGraphics}};
constexpr inline QueueInfo kOneComputeQueue[]  = {{vk::QueueFlagBits::eCompute}};
// One Queue with graphics and compute capabilities
constexpr inline QueueInfo kOneGraphicsComputeQueue[] = {
	{vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute},
};
// One graphics and one separate compute queue
constexpr inline QueueInfo kOneGraphicsOneSeparateComputeQueue[] = {
	{vk::QueueFlagBits::eGraphics},
	{.flags = vk::QueueFlagBits::eCompute, .separate_family = true},
};

inline auto kDefaultRatingFunction(Instance const& instance, PhysicalDevice const& device) {
	float score = 0.0f;
	if (device.GetProperties2().properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
		score += 10.0f;
	}
	if (device.base_features.graphics_pipeline_library.graphicsPipelineLibrary) {
		score += 1.0f;
	}
	return score;
};

// Default staging buffer size
constexpr inline u32 kStagingSize = 64 * 1024 * 1024;

// Graphics pipeline library is preffered by default
constexpr inline const char* const kOptionalDeviceExtensions[] = {
	vk::EXTGraphicsPipelineLibraryExtensionName,
};
} // namespace defaults
} // namespace VB_NAMESPACE
