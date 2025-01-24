#pragma once

#ifndef VB_USE_STD_MODULE
#include <string_view>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/types.hpp"
#include "vulkan_backend/classes/structs.hpp"
#include "vulkan_backend/defaults/image.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
struct ImageInfo {
	u32 static constexpr kBindingNone = ~0u;
	// Necessary data
	Extent3D                     extent;
	vk::Format                   format;
	vk::ImageUsageFlags          usage;
	// Optional
	// bindings to write bindless descriptor to
	u32                          binding = kBindingNone;
	// std::span<u32>               bindings;
	vk::SampleCountFlagBits      samples = vk::SampleCountFlagBits::e1;
	vk::SamplerCreateInfo const& sampler = defaults::linearSampler;
	u32                          layers  = 1;
	std::string_view             name    = "";
};

struct SwapchainImageInfo {
	vk::Image		 vk_image;
	vk::ImageView	 view;
	Extent3D		 extent;
	std::string_view name = "";
};
} // namespace VB_NAMESPACE
