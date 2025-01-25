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

#include "vulkan_backend/interface/image/image.hpp"
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
		Image const&             color_image;
		Image const&             resolve_image = {}; // == no resolve attachment
		vk::AttachmentLoadOp     load_op       = vk::AttachmentLoadOp::eClear;
		vk::AttachmentStoreOp    store_op      = vk::AttachmentStoreOp::eStore;
		vk::ClearColorValue      clear_value   = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
	};

	struct DepthStencilAttachment {
		Image const&                  image;
		vk::AttachmentLoadOp          load_op     = vk::AttachmentLoadOp::eClear;
		vk::AttachmentStoreOp         store_op    = vk::AttachmentStoreOp::eStore;
		vk::ClearDepthStencilValue    clear_value = {1.0f, 0};
	};

	std::span<ColorAttachment const> color_attachments;
	DepthStencilAttachment const&    depth      = {.image = {}};
	DepthStencilAttachment const&    stencil    = {.image = {}};
	vk::Rect2D const&                render_area = vk::Rect2D{};  // == use size of colorAttachs[0] or depth
	u32                              layer_count = 1;
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
} // VB_NAMESPACE

