#pragma once

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"
#include "vulkan_backend/types.hpp"
#include "vulkan_backend/fwd.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
struct SwapchainInfo {
	u32 static constexpr inline kFramesInFlight = 3;
	u32 static constexpr inline kAdditionalImages  = 0;
	// Necessary data
	vk::SurfaceKHR        surface           = nullptr;
	vk::Extent2D          extent            = {};
	u32                   queueFamilyindex  = ~0; // for command buffers
	// Optional
	bool                  destroy_surface   = false;
	u32                   frames_in_flight  = kFramesInFlight;
	u32                   additional_images = kAdditionalImages;
	// Preferred color format, not guaranteed, get actual format after creation with GetFormat()
	vk::Format            preferred_format  = vk::Format::eR8G8B8A8Unorm;
	vk::ColorSpaceKHR     color_space       = vk::ColorSpaceKHR::eSrgbNonlinear;
	vk::PresentModeKHR    present_mode      = vk::PresentModeKHR::eMailbox;
};

} // namespace VB_NAMESPACE
