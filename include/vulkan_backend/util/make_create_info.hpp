#pragma once

#ifndef VB_USE_STD_MODULE
#include <cstdint>
#include <array>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"
#include "vulkan_backend/classes/structs.hpp"

namespace VB_NAMESPACE {
namespace util {
inline auto MakeImageCreateInfo(vk::Format format, Extent3D const& extent, vk::ImageUsageFlags usage) -> vk::ImageCreateInfo {
	return vk::ImageCreateInfo{
		.flags         = {},
		.imageType     = vk::ImageType::e2D,
		.format        = format,
		.extent        = vk::Extent3D(extent),
		.mipLevels     = 1,
		.arrayLayers   = 1,
		.samples       = vk::SampleCountFlagBits::e1,
		.tiling        = vk::ImageTiling::eOptimal,
		.usage         = usage,
		.sharingMode   = vk::SharingMode::eExclusive,
		.initialLayout = vk::ImageLayout::eUndefined,
	};
}
inline auto MakeImageViewCreateInfo(vk::Image image, vk::ImageCreateInfo const& info, vk::ImageAspectFlags aspect)
	-> vk::ImageViewCreateInfo {

	return vk::ImageViewCreateInfo{
		.image    = image,
		.viewType = info.arrayLayers == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::eCube,
		.format   = info.format,
		.subresourceRange{.aspectMask     = aspect,
						  .baseMipLevel   = 0,
						  .levelCount     = info.mipLevels,
						  .baseArrayLayer = 0,
						  .layerCount     = info.arrayLayers}
    };
}

inline auto MakeImageViewCreateInfo(vk::Image image, vk::Format format, vk::ImageAspectFlags aspect) -> vk::ImageViewCreateInfo {
	return vk::ImageViewCreateInfo{
		.flags    = {},
		.image    = image,
		.viewType = vk::ImageViewType::e2D,
		.format   = format,
		.subresourceRange = vk::ImageSubresourceRange{
			.aspectMask     = aspect,
			.baseMipLevel   = 0,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = 1,
		},
	};
}

} // namespace util
} // namespace VB_NAMESPACE