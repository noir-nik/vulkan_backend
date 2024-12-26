#ifndef VULKAN_BACKEND_STRUCTS_HPP_
#define VULKAN_BACKEND_STRUCTS_HPP_

#include <vulkan_backend/enums.hpp>

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <cstdint>
#elif !defined(_VB_INCLUDE_IN_MODULE)
import std;
#endif

#ifdef MemoryBarrier
#undef MemoryBarrier
#endif

VB_EXPORT
namespace VB_NAMESPACE {
using i32 = std::int32_t;
using u8  = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using DeviceSize = u64;

constexpr inline u32        QueueFamilyIgnored = (~0U); 
constexpr inline DeviceSize WholeSize          = (~0ULL);

struct Viewport {
    float    x;
    float    y;
    float    width;
    float    height;
    float    minDepth = 0.0f;
    float    maxDepth = 1.0f;
};

struct Extent2D {
	u32 width  = 0;
	u32 height = 0;
};

struct Extent3D {
	u32 width  = 0;
	u32 height = 0;
	u32 depth  = 1;
};

struct Offset2D {
	i32 x = 0;
	i32 y = 0;
};

struct Offset3D {
	i32 x = 0;
	i32 y = 0;
	i32 z = 0;
};

struct Rect2D {
	Offset2D offset;
	Extent2D extent;
};

union ClearColorValue {
    float float32[4];
    i32   int32[4];
    u32   uint32[4];
};

struct ClearDepthStencilValue {
    float depth;
    u32   stencil;
};

union ClearValue {
    ClearColorValue           color;
    ClearDepthStencilValue    depthStencil;
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

struct SamplerInfo {
	Filter magFilter = Filter::Linear;
	Filter minFilter = Filter::Linear;
	MipmapMode mipmapMode = MipmapMode::Linear;
	WrapMode wrapU = WrapMode::Repeat;
	WrapMode wrapV = WrapMode::Repeat;
	WrapMode wrapW = WrapMode::Repeat;
	float mipLodBias = 0.0f;
	bool anisotropyEnable = false;
	float maxAnisotropy = 0.0f;
	bool compareEnable = false;
	CompareOp compareOp = CompareOp::Always;
	float minLod = 0.0f;
	float maxLod = 1.0f;
	BorderColor borderColor = BorderColor::IntOpaqueBlack;
	bool unnormalizedCoordinates = false;
};

/* ===== Synchronization 2 Barriers ===== */
struct MemoryBarrier {
	PipelineStageFlags srcStageMask  = PipelineStage::AllCommands;
	AccessFlags        srcAccessMask = Access::ShaderWrite;
	PipelineStageFlags dstStageMask  = PipelineStage::AllCommands;
	AccessFlags        dstAccessMask = Access::ShaderRead;
};

struct BufferBarrier {
	u32           srcQueueFamilyIndex = QueueFamilyIgnored;
	u32           dstQueueFamilyIndex = QueueFamilyIgnored;
	DeviceSize    offset              = 0;
	DeviceSize    size                = WholeSize;
	MemoryBarrier memoryBarrier;
};

struct ImageBarrier {
	ImageLayout   newLayout           = ImageLayout::MaxEnum; // == use previous layout
	ImageLayout   oldLayout           = ImageLayout::MaxEnum; // == use previous layout
	u32           srcQueueFamilyIndex = QueueFamilyIgnored;
	u32           dstQueueFamilyIndex = QueueFamilyIgnored;
	MemoryBarrier memoryBarrier;
};

inline bool operator ==(Rect2D const& lhs, Rect2D const& rhs) {
	return lhs.offset.x == rhs.offset.x && lhs.offset.y == rhs.offset.y &&
			lhs.extent.width == rhs.extent.width && lhs.extent.height == rhs.extent.height;
}

} // VB_NAMESPACE
#endif // VULKAN_BACKEND_STRUCTS_HPP_