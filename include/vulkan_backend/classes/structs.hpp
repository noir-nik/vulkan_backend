#pragma once

#ifndef VB_USE_STD_MODULE
#include <cstdint>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"
#include "vulkan_backend/defaults/image.hpp"
#include "vulkan_backend/types.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
namespace Memory {
vk::MemoryPropertyFlags constexpr inline eGPU = vk::MemoryPropertyFlagBits::eDeviceLocal;
vk::MemoryPropertyFlags constexpr inline eCPU =
	vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
} // namespace Memory

struct Viewport {
	float x;
	float y;
	float width;
	float height;
	float minDepth = 0.0f;
	float maxDepth = 1.0f;
};

struct Extent3D {
	u32 width  = 0;
	u32 height = 0;
	u32 depth  = 1;

	operator vk::Extent3D() const { return {width, height, depth}; }

	explicit operator vk::Offset3D() const {
		return {static_cast<int>(width), static_cast<int>(height), static_cast<int>(depth)};
	}

	explicit operator vk::Extent2D() const { return {width, height}; }
};

inline auto Offset3DFromExtent3D(vk::Extent3D const& extent) -> vk::Offset3D {
	return {static_cast<int>(extent.width), static_cast<int>(extent.height), static_cast<int>(extent.depth)};
}

struct ImageBlit {
	// Corners of image e.g. { {0,0,0}, {width, height, depth} }
	struct Region {
		vk::Offset3D offset;
		vk::Offset3D extent;
	};
	Region dst;
	Region src;
};

struct ColorBlend {
	vk::Bool32  logicOpEnable;
	vk::LogicOp logicOp;
	float       blendConstants[4];
};

struct Image2DInfo : public vk::ImageCreateInfo {
	Image2DInfo(Extent3D const& extent, vk::Format const& format, vk::ImageUsageFlags const& usage,
				vk::SampleCountFlagBits const& samples = vk::SampleCountFlagBits::e1,
				vk::SamplerCreateInfo const&   sampler = defaults::linearSampler)
		: vk::ImageCreateInfo{defaults::image2D} {
		this->extent  = extent;
		this->format  = format;
		this->usage   = usage;
		this->sampler = sampler;
		this->samples = samples;
	}
	vk::SamplerCreateInfo& sampler = defaults::linearSampler;
};

} // namespace VB_NAMESPACE
