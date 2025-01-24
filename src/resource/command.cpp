#ifndef VB_USE_STD_MODULE
#include <utility>
#include <limits>
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/interface/command.hpp"
#include "vulkan_backend/interface/instance.hpp"
#include "vulkan_backend/interface/device.hpp"
#include "vulkan_backend/interface/buffer.hpp"
#include "vulkan_backend/interface/image.hpp"
#include "vulkan_backend/interface/pipeline.hpp"
#include "vulkan_backend/interface/descriptor.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/vk_result.hpp"
#include "vulkan_backend/log.hpp"


namespace VB_NAMESPACE {
Command::Command(std::shared_ptr<Device> const& device, u32 queueFamilyindex) : ResourceBase(device) {
	Create(queueFamilyindex);
}
Command::Command(Command&& other)
		: vk::CommandBuffer(std::exchange(other, {})), ResourceBase(std::move(other)),
		  pool(std::exchange(other.pool, {})), fence(std::exchange(other.fence, {})) {}

Command& Command::operator=(Command&& other) {
	vk::CommandBuffer::operator=(std::exchange(other, {}));
	ResourceBase::operator=(std::move(other));
	pool = std::exchange(other.pool, {});
	fence = std::exchange(other.fence, {});
	return *this;
}

Command::~Command() { Free(); }
	
bool Command::Copy(vk::Buffer const& dst, void const* data, u32 size, u32 dstOfsset) {
	auto device = owner;
	VB_LOG_TRACE("[ CmdCopy ] size: %u, offset: %u", size, device->GetStagingOffset());
	if (device->GetStagingSize() - device->GetStagingOffset() < size) [[unlikely]] {
		VB_LOG_WARN("Not enough size in staging buffer to copy");
		return false;
	}
	vmaCopyMemoryToAllocation(owner->vma_allocator, data, device->staging.allocation, device->GetStagingOffset(), size);
	Copy(dst, device->staging, size, dstOfsset, device->GetStagingOffset());
	device->staging_offset += size;
	return true;
}

void Command::Copy(vk::Buffer const& dst, vk::Buffer const& src, u32 size, u32 dstOffset, u32 srcOffset) {
	vk::BufferCopy2 copyRegion{
		.pNext = nullptr,
		.srcOffset = srcOffset,
		.dstOffset = dstOffset,
		.size = size,
	};

	vk::CopyBufferInfo2 copyBufferInfo{
		.srcBuffer = src,
		.dstBuffer = dst,
		.regionCount = 1,
		.pRegions = &copyRegion
	};

	copyBuffer2(&copyBufferInfo);
}

bool Command::Copy(Image const& dst, void const* data, u32 size) {
	auto device = owner;
	VB_LOG_TRACE("[ CmdCopy ] size: %u, offset: %u", size, device->GetStagingOffset());
	if (device->GetStagingSize() - device->GetStagingOffset() < size) [[unlikely]] {
		VB_LOG_WARN("Not enough size in staging buffer to copy");
		return false;
	}
	std::memcpy(device->staging_cpu + device->GetStagingOffset(), data, size);
	Copy(dst, device->staging, device->GetStagingOffset());
	device->staging_offset += size;
	return true;
}

void Command::Copy(Image const &dst, vk::Buffer const &src, u32 srcOffset) {
	// VB_ASSERT(!(dst.aspect & vk::ImageAspectFlagBits::eDepth ||
	// 			dst.aspect & vk::ImageAspectFlagBits::eStencil),
	// 		  "CmdCopy doesnt't support depth/stencil images");
	vk::BufferImageCopy2 region{
		.pNext = nullptr,
		.bufferOffset = srcOffset,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource{
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.imageOffset = {0, 0, 0},
		.imageExtent = dst.info.extent,
	};

	vk::CopyBufferToImageInfo2 copyBufferToImageInfo{
		.srcBuffer = src,
		.dstImage = dst,
		.dstImageLayout = dst.layout,
		.regionCount = 1,
		.pRegions = &region
	};

	copyBufferToImage2(&copyBufferToImageInfo);
}

void Command::Copy(vk::Buffer const &dst, Image const &src, u32 dstOffset,
				vk::Offset3D imageOffset, Extent3D imageExtent) {
	// VB_ASSERT(!(src.aspect & vk::ImageAspectFlagBits::eDepth ||
	// 			src.aspect & vk::ImageAspectFlagBits::eStencil),
	// 		"CmdCopy doesn't support depth/stencil images");
	vk::BufferImageCopy2 region{
		.bufferOffset = dstOffset,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource{
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.imageOffset = {imageOffset.x, imageOffset.y, imageOffset.z},
		.imageExtent = src.info.extent
	};

	vk::CopyImageToBufferInfo2 copyInfo{
		.srcImage = src,
		.srcImageLayout = (vk::ImageLayout)src.layout,
		.dstBuffer = dst,
		.regionCount = 1,
		.pRegions = &region,
	};
	copyImageToBuffer2(&copyInfo);
}

void Command::Barrier(Image& img, ImageBarrier const& barrier) {
	vk::ImageSubresourceRange range = {
		.aspectMask = img.aspect,
		.baseMipLevel = 0,
		.levelCount = vk::RemainingMipLevels,
		.baseArrayLayer = 0,
		.layerCount = vk::RemainingArrayLayers,
	};

	vk::ImageMemoryBarrier2 barrier2 = {
		.pNext               = nullptr,
		.srcStageMask        =   barrier.memoryBarrier.srcStageMask,
		.srcAccessMask       =          barrier.memoryBarrier.srcAccessMask,
		.dstStageMask        =   barrier.memoryBarrier.dstStageMask,
		.dstAccessMask       =          barrier.memoryBarrier.dstAccessMask,
		.oldLayout           = (barrier.oldLayout == vk::ImageLayout::eUndefined
									? img.layout
									: barrier.oldLayout),
		.newLayout           = (barrier.newLayout == vk::ImageLayout::eUndefined
									? img.layout
									: barrier.newLayout),
		.srcQueueFamilyIndex = barrier.srcQueueFamilyIndex,
		.dstQueueFamilyIndex = barrier.dstQueueFamilyIndex,
		.image               = img,
		.subresourceRange    = range
	};

	vk::DependencyInfo dependency = {
		.pNext                    = nullptr,
		.dependencyFlags          = {},
		.memoryBarrierCount       = 0,
		.pMemoryBarriers          = nullptr,
		.bufferMemoryBarrierCount = 0,
		.pBufferMemoryBarriers    = nullptr,
		.imageMemoryBarrierCount  = 1,
		.pImageMemoryBarriers     = &barrier2
	};

	pipelineBarrier2(&dependency);
	img.layout = barrier.newLayout;
}

void Command::Barrier(vk::Buffer const& buf, BufferBarrier const& barrier) {
	vk::BufferMemoryBarrier2 barrier2 {
		.pNext               = nullptr,
		.srcStageMask        = (vk::PipelineStageFlags2) barrier.memoryBarrier.srcStageMask,
		.srcAccessMask       = (vk::AccessFlags2)        barrier.memoryBarrier.srcAccessMask,
		.dstStageMask        = (vk::PipelineStageFlags2) barrier.memoryBarrier.dstStageMask,
		.dstAccessMask       = (vk::AccessFlags2)        barrier.memoryBarrier.dstAccessMask,
		.srcQueueFamilyIndex =                           barrier.srcQueueFamilyIndex,
		.dstQueueFamilyIndex =                           barrier.dstQueueFamilyIndex,
		.buffer              =                           buf,
		.offset              = (vk::DeviceSize)          barrier.offset,
		.size                = (vk::DeviceSize)          barrier.size
	};

	vk::DependencyInfo dependency = {
		.pNext                    = nullptr,
		.dependencyFlags          = {},
		.bufferMemoryBarrierCount = 1,
		.pBufferMemoryBarriers    = &barrier2,
	};
	pipelineBarrier2(&dependency);
}

void Command::Barrier(MemoryBarrier const& barrier) {
	vk::MemoryBarrier2 barrier2 = {
		.pNext         = nullptr,
		.srcStageMask  = (vk::PipelineStageFlags2) barrier.srcStageMask,
		.srcAccessMask = (vk::AccessFlags2)        barrier.srcAccessMask,
		.dstStageMask  = (vk::PipelineStageFlags2) barrier.dstStageMask,
		.dstAccessMask = (vk::AccessFlags2)        barrier.dstAccessMask
	};
	vk::DependencyInfo dependency = {
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &barrier2,
	};
	pipelineBarrier2(&dependency);
}

void Command::ClearColorImage(Image const& img, vk::ClearColorValue const& color) {
	vk::ClearColorValue clearColor{{{color.float32[0], color.float32[1], color.float32[2], color.float32[3]}}};
	vk::ImageSubresourceRange range = {
		.aspectMask = (vk::ImageAspectFlags)img.aspect,
		.baseMipLevel = 0,
		.levelCount = vk::RemainingMipLevels,
		.baseArrayLayer = 0,
		.layerCount = vk::RemainingArrayLayers,
	};

	clearColorImage(img,
		(vk::ImageLayout)img.layout, &clearColor, 1, &range);
}

void Command::Blit(BlitInfo const& info) {
	auto regions = info.regions;

	ImageBlit const fullRegions[] = {{
		{{0, 0, 0}, vk::Offset3D(info.dst.info.extent)},
		{{0, 0, 0}, vk::Offset3D(info.src.info.extent)}
	}};

	if (regions.empty()) {
		regions = fullRegions;
	}

	VB_VLA(vk::ImageBlit2, blitRegions, regions.size());
	for (auto i = 0; auto& region: regions) {
		blitRegions[i] = {
			.pNext = nullptr,
			.srcSubresource{
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel       = 0,
				.baseArrayLayer = 0,
				.layerCount     = 1,
			},
			.srcOffsets = {vk::Offset3D(region.src.offset), vk::Offset3D(region.src.extent)},
			.dstSubresource{
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel       = 0,
				.baseArrayLayer = 0,
				.layerCount     = 1,
			},
			.dstOffsets = {vk::Offset3D(region.dst.offset), vk::Offset3D(region.dst.extent)},
		};
		++i;
	}

	vk::BlitImageInfo2 blitInfo {
		.pNext          = nullptr,
		.srcImage       = info.src,
		.srcImageLayout = info.src.layout,
		.dstImage       = info.dst,
		.dstImageLayout = info.dst.layout,
		.regionCount    = static_cast<u32>(blitRegions.size()),
		.pRegions       = blitRegions.data(),
		.filter         = info.filter,
	};

	blitImage2(&blitInfo);
}

void Command::BeginRendering(RenderingInfo const& info) {
	// auto& clearColor = info.clearColor;
	// auto& clearDepth = info.clearDepth;
	// auto& clearStencil = info.clearStencil;

	vk::Rect2D renderArea = info.renderArea;
	if (renderArea == vk::Rect2D{}) {
		if (info.colorAttachs.size() > 0) {
			renderArea.extent.width = info.colorAttachs[0].colorImage.info.extent.width;
			renderArea.extent.height = info.colorAttachs[0].colorImage.info.extent.height;
		} else if (info.depth.image) {
			renderArea.extent.width = info.depth.image.info.extent.width;
			renderArea.extent.height = info.depth.image.info.extent.height;
		}
	}

	vk::Offset2D offset(renderArea.offset.x, renderArea.offset.y);
	vk::Extent2D extent(renderArea.extent.width, renderArea.extent.height);

	VB_VLA(vk::RenderingAttachmentInfo, colorAttachInfos, info.colorAttachs.size());
	for (auto i = 0; auto& colorAttach: info.colorAttachs) {
		colorAttachInfos[i] = {
			.imageView   = colorAttach.colorImage.view,
			.imageLayout = colorAttach.colorImage.layout,
			.loadOp      = colorAttach.loadOp,
			.storeOp     = colorAttach.storeOp,
			.clearValue  = *reinterpret_cast<vk::ClearValue const*>(&colorAttach.clearValue),
		};
		if (colorAttach.resolveImage) {
			colorAttachInfos[i].resolveMode        = vk::ResolveModeFlagBits::eAverage;
			colorAttachInfos[i].resolveImageView   = colorAttach.resolveImage.view;
			colorAttachInfos[i].resolveImageLayout = vk::ImageLayout(colorAttach.resolveImage.layout);
		}
		++i;
	}

	vk::RenderingInfoKHR renderingInfo{
		.flags = {},
		.renderArea = {
			.offset = offset,
			.extent = extent,
		},
		.layerCount = info.layerCount,
		.viewMask = 0,
		.colorAttachmentCount = static_cast<u32>(colorAttachInfos.size()),
		.pColorAttachments = colorAttachInfos.data(),
		.pDepthAttachment = nullptr,
		.pStencilAttachment = nullptr,
	};

	vk::RenderingAttachmentInfo depthAttachInfo;
	if (info.depth.image) {
		depthAttachInfo = {
			.imageView   = info.depth.image.view,
			.imageLayout = info.depth.image.layout,
			.loadOp      = info.depth.loadOp,
			.storeOp     = info.depth.storeOp,
			// .storeOp = info.depth.image.usage & vk::ImageUsageFlagBits::eTransientAttachment 
			//	? vk::AttachmentStoreOp::eDontCare
			//	: vk::AttachmentStoreOp::eStore,
			.clearValue {
				.depthStencil = { info.depth.clearValue.depth, info.depth.clearValue.stencil }
			}
		};
		renderingInfo.pDepthAttachment = &depthAttachInfo;
	}
	vk::RenderingAttachmentInfo stencilAttachInfo;
	if (info.stencil.image) {
		stencilAttachInfo = {
			.imageView   = info.stencil.image.view,
			.imageLayout = info.stencil.image.layout,
			.loadOp      = info.stencil.loadOp,
			.storeOp     = info.stencil.storeOp,
			.clearValue {
				.depthStencil = { info.stencil.clearValue.depth, info.stencil.clearValue.stencil }
			}
		};
		renderingInfo.pStencilAttachment = &stencilAttachInfo;
	}
	beginRendering(&renderingInfo);
}

void Command::SetViewport(Viewport const& viewport) {
	vk::Viewport vkViewport {
		.x        = viewport.x,
		.y        = viewport.y,
		.width    = viewport.width,
		.height   = viewport.height,
		.minDepth = viewport.minDepth,
		.maxDepth = viewport.maxDepth,
	};
	setViewport(0, 1, &vkViewport);
}

void Command::SetScissor(vk::Rect2D const& scissor) {
	vk::Rect2D vkScissor {
		.offset = { scissor.offset.x, scissor.offset.y },
		.extent = {
			.width  = scissor.extent.width,
			.height = scissor.extent.height
		}
	};
	setScissor(0, 1, &vkScissor);
}

void Command::EndRendering() {
	endRendering();
}

void Command::BindPipeline(Pipeline const& pipeline) {
	bindPipeline(pipeline.point, pipeline);
	// TODO(nm): bind only if not compatible for used descriptor sets or push constant range
	// ref:
	// https://registry.khronos.org/vulkan/specs/1.2-extensions/html/vkspec.html#descriptorsets-compatibility
	bindDescriptorSets(pipeline.point, pipeline.layout, 0, 1, &owner->descriptor.set, 0, nullptr);
}

void Command::PushConstants(Pipeline const& pipeline, void const* data, u32 size) {
	pushConstants(pipeline.layout, vk::ShaderStageFlagBits::eAll, 0, size, data);
}

void Command::BindVertexBuffer(Buffer const& vertex_buffer) {
	vk::DeviceSize offsets[] = { 0 };
	bindVertexBuffers(0, 1, &vertex_buffer, offsets);
}

void Command::BindIndexBuffer(Buffer const& index_buffer) {
	bindIndexBuffer(index_buffer, 0, vk::IndexType::eUint32);
}

void Command::Draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance) {
	// VB_LOG_TRACE("CmdDraw(%u,%u,%u,%u)", vertex_count, instance_count, first_vertex, first_instance);
	draw(vertex_count, instance_count, first_vertex, first_instance);
}

void Command::DrawIndexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance) {
	// VB_LOG_TRACE("CmdDrawIndexed(%u,%u,%u,%u,%u)", index_count, instance_count, first_index, vertex_offset, first_instance);
	drawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
}

void Command::DrawMesh(Buffer const& vertex_buffer, Buffer const& index_buffer, u32 index_count) {
	vk::DeviceSize offsets[] = { 0 };
	bindVertexBuffers(0, 1, &vertex_buffer, offsets);
	bindIndexBuffer(index_buffer, 0, vk::IndexType::eUint32);
	drawIndexed(index_count, 1, 0, 0, 0);
}

void Command::Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) {
	dispatch(groupCountX, groupCountY, groupCountZ);
}

// vkWaitForFences + vkResetFences +
// vkResetCommandPool + vkBeginCommandBuffer
void Command::Begin() {
	VB_VK_RESULT result;
	result = owner->waitForFences(1, &fence, vk::True, std::numeric_limits<u64>::max());
	VB_CHECK_VK_RESULT(result, "Failed to wait for fence");
	result = owner->resetFences(1, &fence);
	VB_CHECK_VK_RESULT(result, "Failed to reset fence");

	// ?VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT
	result = owner->resetCommandPool(pool, vk::CommandPoolResetFlags{});
	VB_CHECK_VK_RESULT(result, "Failed to reset command pool");
	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	result = begin(&beginInfo);
}

void Command::End() {
	VB_VK_RESULT result = end();
	VB_CHECK_VK_RESULT(result, "Failed to end command buffer");
}

void Command::QueueSubmit(vk::Queue const& queue, SubmitInfo const& info) {

	vk::CommandBufferSubmitInfo cmdInfo {
		.commandBuffer = *this,
	};

	vk::SubmitInfo2 submitInfo {
		.waitSemaphoreInfoCount = static_cast<u32>(info.waitSemaphoreInfos.size()),
		.pWaitSemaphoreInfos = reinterpret_cast<vk::SemaphoreSubmitInfo const*>(info.waitSemaphoreInfos.data()),
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmdInfo,
		.signalSemaphoreInfoCount = static_cast<u32>(info.signalSemaphoreInfos.size()),
		.pSignalSemaphoreInfos = reinterpret_cast<vk::SemaphoreSubmitInfo const*>(info.signalSemaphoreInfos.data()),
	};

	auto result = queue.submit2(1, &submitInfo, fence);
	VB_CHECK_VK_RESULT(result, "Failed to submit command buffer");

	// Commented code can be slower
	// queue.Submit(
	// 	{{{.commandBuffer = *this}}},
	// 	fence, {
	// 		.waitSemaphoreInfos = info.waitSemaphoreInfos,
	// 		.signalSemaphoreInfos = info.signalSemaphoreInfos,
	// 	});
}


auto Command::GetFence() const -> vk::Fence {
	return fence;
}

void Command::Create(u32 queueFamilyindex) {
	vk::CommandPoolCreateInfo poolInfo {
		// .flags = 0, // do not use VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
		.queueFamilyIndex = queueFamilyindex
	};

	vk::Result result;
	std::tie(result, pool) = owner->createCommandPool(poolInfo, owner->owner->allocator);
	VB_CHECK_VK_RESULT(result, "Failed to create command pool!");


	vk::CommandBufferAllocateInfo allocInfo {
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,
	};
	allocInfo.commandPool = pool;

	vk::CommandBuffer commandBuffer;
	result = owner->allocateCommandBuffers(&allocInfo, &commandBuffer);
	vk::CommandBuffer::operator=(commandBuffer);
	
	VB_CHECK_VK_RESULT(result, "Failed to allocate command buffer!");

	vk::FenceCreateInfo fenceInfo{
		.flags = vk::FenceCreateFlagBits::eSignaled,
	};

	std::tie(result, fence) = owner->createFence(fenceInfo, owner->owner->allocator);
	VB_CHECK_VK_RESULT(result, "Failed to create fence!");
}

auto Command::GetResourceTypeName() const -> char const* { return "CommandResource"; }

// auto Command::GetInstance() const -> Instance* { return owner->owner; }

void Command::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), "Command");
	owner->destroyCommandPool(pool, owner->owner->allocator);
	owner->destroyFence(fence, owner->owner->allocator);
}
} // namespace VB_NAMESPACE
