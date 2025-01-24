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
#include "vulkan_backend/types.hpp"
#include "vulkan_backend/defaults/image.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
enum class Memory {
	GPU = vk::MemoryPropertyFlags::MaskType(vk::MemoryPropertyFlagBits::eDeviceLocal),
	CPU = vk::MemoryPropertyFlags::MaskType(vk::MemoryPropertyFlagBits::eHostVisible)
		| vk::MemoryPropertyFlags::MaskType(vk::MemoryPropertyFlagBits::eHostCoherent),
};

using MemoryFlags = vk::Flags<Memory>;

struct Viewport {
    float    x;
    float    y;
    float    width;
    float    height;
    float    minDepth = 0.0f;
    float    maxDepth = 1.0f;
};

struct Extent3D {
	u32 width  = 0;
	u32 height = 0;
	u32 depth  = 1;

	operator vk::Extent3D() const {
		return {width, height, depth};
	}

	explicit operator vk::Offset3D() const {
		return {static_cast<int>(width),
				static_cast<int>(height),
				static_cast<int>(depth)};
	}

	explicit operator vk::Extent2D() const {
		return {width, height};
	}
};

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
	float	    blendConstants[4];
};

struct Image2DInfo : public vk::ImageCreateInfo {
	Image2DInfo(Extent3D const& extent,
				vk::Format const& format,
				vk::ImageUsageFlags const& usage,
				vk::SampleCountFlagBits const& samples = vk::SampleCountFlagBits::e1,
				vk::SamplerCreateInfo const&   sampler = defaults::linearSampler)
		: vk::ImageCreateInfo{defaults::image2D} {
		this->extent = extent;
		this->format = format;
		this->usage  = usage;
		this->sampler = sampler;
		this->samples = samples;
		}
	vk::SamplerCreateInfo& sampler = defaults::linearSampler;
};

struct ImGuiInitInfo {
	vk::Instance                      Instance;
	vk::PhysicalDevice                PhysicalDevice;
	vk::Device                        Device;
	vk::DescriptorPool                DescriptorPool;
	u32                               MinImageCount;
	u32                               ImageCount;
	vk::PipelineCache                 PipelineCache;
	bool                              UseDynamicRendering;
	const vk::AllocationCallbacks*    Allocator;
};

} // VB_NAMESPACE