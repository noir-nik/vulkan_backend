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

#include "vulkan_backend/classes/structs.hpp"
#include "vulkan_backend/defaults/image.hpp"
#include "vulkan_backend/interface/descriptor/descriptor.hpp"
#include "vulkan_backend/types.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
struct ImageInfo {
	vk::ImageCreateInfo        create_info;
	vk::ImageAspectFlags const aspect;
	std::string_view const     name             = "";
	bool                       check_vk_results = true;
};

struct BindlessImageInfo {
	ImageInfo                  image_info;
	u32 const                  binding;
	vk::ImageLayout const      layout;
	vk::Sampler const          sampler;
};

struct SwapchainImageInfo {
	vk::Image        vk_image;
	vk::ImageView    view;
	Extent3D         extent;
	std::string_view name = "";
};
} // namespace VB_NAMESPACE
