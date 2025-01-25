#pragma once

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"

VB_EXPORT
namespace VB_NAMESPACE {	
struct QueueInfo {
	static constexpr vk::QueueFlags kUndesiredFlagsForDedicatedCompute = vk::QueueFlagBits::eGraphics;
	static constexpr vk::QueueFlags kUndesiredFlagsForDedicatedTransfer = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
	// More flags from respective queue family are added when queue is created
	vk::QueueFlags    flags             = vk::QueueFlagBits::eGraphics;
	// flags are ignored if flags_extra is specified on creation
	vk::QueueFlags    undesired_flags   = {};
	// Require surface support
	vk::SurfaceKHR    supported_surface = nullptr;
};
} // namespace VB_NAMESPACE
