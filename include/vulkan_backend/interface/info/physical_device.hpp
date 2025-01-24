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
#include "vulkan_backend/interface/physical_device.hpp"
#include "vulkan_backend/defaults/device.hpp"
#include "queue.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
struct PhysicalDeviceSelectInfo {
	// Required user extensions for the physical device
	std::span<char const* const> extensions;
	// Span of required queues, defaulting to one queue with graphics capability
	std::span<QueueInfo const> queues = defaults::kOneGraphicsComputeQueue;
	// Optional function that returns the biggest number for the most preferred device
	// Return negative value to reject device
	// std::function<float(Instance const&, PhysicalDevice const&)> rating_function = defaults::kDefaultRatingFunction;
	float (& rating_function)(Instance const&, PhysicalDevice const&) = defaults::kDefaultRatingFunction;
};

// struct PhysicalDeviceSelectResult {
// 	PhysicalDevice* physical_device;
// 	std::vector<vk::DeviceQueueCreateInfo> queues_to_create;
// };
} // namespace VB_NAMESPACE
