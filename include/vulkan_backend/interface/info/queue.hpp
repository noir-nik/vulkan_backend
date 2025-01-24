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
	// More flags from respective queue family are added when queue is created
	vk::QueueFlags    flags             = vk::QueueFlagBits::eGraphics;
	bool              separate_family   = false;    // Prefer separate family
	vk::SurfaceKHR    supported_surface = nullptr;
};
} // namespace VB_NAMESPACE
