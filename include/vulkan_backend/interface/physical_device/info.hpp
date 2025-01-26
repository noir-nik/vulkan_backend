#pragma once

#ifndef VB_USE_STD_MODULE
#include <span>
#include <functional>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/types.hpp"
#include "vulkan_backend/defaults/device.hpp"
#include "vulkan_backend/interface/queue/queue.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
struct PhysicalDeviceSupportInfo {
	// Required user extensions for the physical device
	std::span<char const* const> extensions = {};
	// Span of required queues, defaulting to one queue with graphics capability
	std::span<QueueInfo const> queues = defaults::kOneGraphicsComputeQueue;
};

// struct PhysicalDeviceSelectResult {
// 	PhysicalDevice* physical_device;
// 	std::vector<vk::DeviceQueueCreateInfo> queues_to_create;
// };
} // namespace VB_NAMESPACE
