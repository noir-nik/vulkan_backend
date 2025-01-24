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

#include "vulkan_backend/interface/image.hpp"
#include "vulkan_backend/config.hpp"
#include "vulkan_backend/types.hpp"

VB_EXPORT
namespace VB_NAMESPACE {

struct SubmitInfo {
	std::span<vk::SemaphoreSubmitInfo const> waitSemaphoreInfos;
	std::span<vk::SemaphoreSubmitInfo const> signalSemaphoreInfos;
};

struct RenderingInfo {
	struct ColorAttachment {
		Image const&             colorImage;
		Image const&             resolveImage = {}; // == no resolve attachment
		vk::AttachmentLoadOp     loadOp       = vk::AttachmentLoadOp::eClear;
		vk::AttachmentStoreOp    storeOp      = vk::AttachmentStoreOp::eStore;
		vk::ClearColorValue      clearValue   = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
	};

	struct DepthStencilAttachment {
		Image const&                  image;
		vk::AttachmentLoadOp          loadOp     = vk::AttachmentLoadOp::eClear;
		vk::AttachmentStoreOp         storeOp    = vk::AttachmentStoreOp::eStore;
		vk::ClearDepthStencilValue    clearValue = {1.0f, 0};
	};

	std::span<ColorAttachment const> colorAttachs;
	DepthStencilAttachment const&    depth      = {.image = {}};
	DepthStencilAttachment const&    stencil    = {.image = {}};
	vk::Rect2D const&                renderArea = vk::Rect2D{};  // == use size of colorAttachs[0] or depth
	u32                              layerCount = 1;
};

struct BlitInfo {
	Image const&               dst;
	Image const&               src;
	std::span<ImageBlit const> regions = {};
	vk::Filter                 filter  = vk::Filter::eLinear;
};

/* ===== Synchronization 2 Barriers ===== */
struct MemoryBarrier {
	vk::PipelineStageFlags2 srcStageMask  = vk::PipelineStageFlagBits2::eAllCommands;
	vk::AccessFlags2        srcAccessMask = vk::AccessFlagBits2::eShaderWrite;
	vk::PipelineStageFlags2 dstStageMask  = vk::PipelineStageFlagBits2::eAllCommands;
	vk::AccessFlags2        dstAccessMask = vk::AccessFlagBits2::eShaderRead;
};

struct BufferBarrier {
	u32            srcQueueFamilyIndex = vk::QueueFamilyIgnored;
	u32            dstQueueFamilyIndex = vk::QueueFamilyIgnored;
	vk::DeviceSize offset              = 0;
	vk::DeviceSize size                = vk::WholeSize;
	MemoryBarrier  memoryBarrier;
};

struct ImageBarrier {
	vk::ImageLayout newLayout           = vk::ImageLayout::eUndefined; // == use previous layout
	vk::ImageLayout oldLayout           = vk::ImageLayout::eUndefined; // == use previous layout
	u32             srcQueueFamilyIndex = vk::QueueFamilyIgnored;
	u32             dstQueueFamilyIndex = vk::QueueFamilyIgnored;
	MemoryBarrier   memoryBarrier;
};

// this is rather complicated, but necessary
struct QueueFamilyIndexRequest {
	enum class Result {
		kSuccess,
		kNoSuitableQueue,
		kAllSuitableQueuesTaken
	};
	
	struct AvoidInfo{
		vk::QueueFlags flags;
		float penalty;
	};
	// Default avoid flags for compute, transfer and all other queues
	// to select the best queue
	AvoidInfo static constexpr inline kAvoidCompute[] = 
		{{vk::QueueFlagBits::eGraphics, 1.0f}, {vk::QueueFlagBits::eTransfer, 0.5f}};
	AvoidInfo static constexpr inline kAvoidTransfer[] = 
		{{vk::QueueFlagBits::eGraphics, 1.0f}, {vk::QueueFlagBits::eCompute, 0.5f}};
	AvoidInfo static constexpr inline kAvoidOther[] = 
		{{vk::QueueFlagBits::eGraphics, 1.0f}};

	// required queue flags (strict)
	vk::QueueFlags desired_flags;
	// undesired queue flags (strict)
	vk::QueueFlags undesired_flags;
	// span of pairs (flags, priority to avoid)
	std::span<AvoidInfo const> avoid_if_possible = {};
	// surface to support (strict)
	vk::SurfaceKHR surface_to_support = nullptr;
	// numbers of already taken queues in families (optional)
	std::span<u32 const> num_taken_queues = {};
};
} // VB_NAMESPACE

