#pragma once

#ifndef VB_USE_STD_MODULE
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
namespace defaults {
vk::SamplerCreateInfo inline linearSampler = {
	.magFilter               = vk::Filter::eLinear,
	.minFilter               = vk::Filter::eLinear,
	.mipmapMode              = vk::SamplerMipmapMode::eLinear,
	.addressModeU            = vk::SamplerAddressMode::eRepeat,
	.addressModeV            = vk::SamplerAddressMode::eRepeat,
	.addressModeW            = vk::SamplerAddressMode::eRepeat,
	.mipLodBias              = 0.0f,
	.anisotropyEnable        = vk::False,
	.maxAnisotropy           = 0.0f,
	.compareEnable           = vk::False,
	.compareOp               = vk::CompareOp::eAlways,
	.minLod                  = 0.0f,
	.maxLod                  = 1.0f,
	.borderColor             = vk::BorderColor::eIntOpaqueBlack,
	.unnormalizedCoordinates = vk::False,
};

vk::ImageCreateInfo inline image2D = {
	.flags                 = vk::ImageCreateFlags{},
	.imageType             = vk::ImageType::e2D,
	.format                = vk::Format::eR8G8B8A8Unorm,
	.extent                = vk::Extent3D{1, 1, 1},
	.mipLevels             = 1,
	.arrayLayers           = 1,
	.samples               = vk::SampleCountFlagBits::e1,
	.usage                 = vk::ImageUsageFlagBits::eSampled,
};

} // namespace defaults
} // namespace VB_NAMESPACE
