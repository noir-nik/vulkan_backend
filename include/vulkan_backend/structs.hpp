#ifndef VULKAN_BACKEND_STRUCTS_HPP_
#define VULKAN_BACKEND_STRUCTS_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <cstdint>
#elif !defined(_VB_INCLUDE_IN_MODULE)
import std;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif !defined(_VB_INCLUDE_IN_MODULE)
import vulkan_hpp;
#endif

#include <vulkan_backend/config.hpp>

#ifdef MemoryBarrier
#undef MemoryBarrier
#endif

VB_EXPORT
namespace VB_NAMESPACE {
using i32 = std::int32_t;
using u8  = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using vk::Extent2D;
using vk::Extent3D;
using vk::Offset2D;
using vk::Offset3D;
using vk::Rect2D;

using vk::QueueFamilyIgnored;
using vk::DeviceSize;
using vk::WholeSize;
using vk::ImageLayout;

using PipelineStageFlags = vk::PipelineStageFlags2;
using PipelineStage = vk::PipelineStageFlagBits2;
using Access = vk::AccessFlagBits2;
using AccessFlags = vk::AccessFlags;
using SampleCount = vk::SampleCountFlagBits;
using Filter = vk::Filter;
using MipmapMode = vk::SamplerMipmapMode;
using WrapMode = vk::SamplerAddressMode;
using CompareOp = vk::CompareOp;
using BorderColor = vk::BorderColor;

struct Viewport {
    float    x;
    float    y;
    float    width;
    float    height;
    float    minDepth = 0.0f;
    float    maxDepth = 1.0f;
};
using vk::ClearColorValue;
using vk::ClearDepthStencilValue;
using vk::ClearValue;
// union ClearColorValue {
//     float float32[4];
//     i32   int32[4];
//     u32   uint32[4];
// };

// struct ClearDepthStencilValue {
//     float depth;
//     u32   stencil;
// };

// union ClearValue {
//     ClearColorValue           color;
//     ClearDepthStencilValue    depthStencil;
// };

struct ImageBlit {
	// Corners of image e.g. { {0,0,0}, {width, height, depth} }
	struct Region { 
		Offset3D offset;
		Offset3D extent;
	};
	Region dst;
	Region src;
};

struct SamplerInfo {
	Filter magFilter = Filter::eLinear;
	Filter minFilter = Filter::eLinear;
	MipmapMode mipmapMode = MipmapMode::eLinear;
	WrapMode wrapU = WrapMode::eRepeat;
	WrapMode wrapV = WrapMode::eRepeat;
	WrapMode wrapW = WrapMode::eRepeat;
	float mipLodBias = 0.0f;
	bool anisotropyEnable = false;
	float maxAnisotropy = 0.0f;
	bool compareEnable = false;
	CompareOp compareOp = CompareOp::eAlways;
	float minLod = 0.0f;
	float maxLod = 1.0f;
	BorderColor borderColor = BorderColor::eIntOpaqueBlack;
	bool unnormalizedCoordinates = false;
};

/* ===== Synchronization 2 Barriers ===== */
struct MemoryBarrier {
	PipelineStageFlags srcStageMask  = PipelineStage::eAllCommands;
	vk::AccessFlags2        srcAccessMask = Access::eShaderWrite;
	PipelineStageFlags dstStageMask  = PipelineStage::eAllCommands;
	vk::AccessFlags2        dstAccessMask = Access::eShaderRead;
};

struct BufferBarrier {
	u32            srcQueueFamilyIndex = QueueFamilyIgnored;
	u32            dstQueueFamilyIndex = QueueFamilyIgnored;
	DeviceSize offset              = 0;
	DeviceSize size                = WholeSize;
	MemoryBarrier  memoryBarrier;
};

struct ImageBarrier {
	ImageLayout newLayout           = ImageLayout::eUndefined; // == use previous layout
	ImageLayout oldLayout           = ImageLayout::eUndefined; // == use previous layout
	u32             srcQueueFamilyIndex = QueueFamilyIgnored;
	u32             dstQueueFamilyIndex = QueueFamilyIgnored;
	MemoryBarrier memoryBarrier;
};

} // VB_NAMESPACE
#endif // VULKAN_BACKEND_STRUCTS_HPP_