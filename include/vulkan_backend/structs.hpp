#ifndef VULKAN_BACKEND_STRUCTS_HPP_
#define VULKAN_BACKEND_STRUCTS_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <cstdint>
#elif defined(VB_DEV)
import std;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include <vulkan_backend/config.hpp>
#include "fwd.hpp"

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

	operator vk::Extent3D(){
		return {width, height, depth};
	}

	explicit operator Offset3D(){
		return {
		static_cast<int>(width),
		static_cast<int>(height),
		static_cast<int>(depth)
		};
	}
};

struct ImageBlit {
	// Corners of image e.g. { {0,0,0}, {width, height, depth} }
	struct Region { 
		Offset3D offset;
		Offset3D extent;
	};
	Region dst;
	Region src;
};

} // VB_NAMESPACE
#endif // VULKAN_BACKEND_STRUCTS_HPP_