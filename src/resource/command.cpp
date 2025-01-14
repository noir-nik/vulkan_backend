#ifndef VB_USE_STD_MODULE
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/interface/command.hpp"
#include "command.hpp"
#include "instance.hpp"
#include "device.hpp"
#include "buffer.hpp"
#include "image.hpp"
#include "pipeline.hpp"
#include "descriptor.hpp"
#include "../macros.hpp"


namespace VB_NAMESPACE {
bool Command::Copy(Buffer const& dst, void const* data, u32 size, u32 dstOfsset) {
	auto device = resource->owner;
	auto GetInstance = [&cmd = *this->resource]() -> InstanceResource* { return cmd.GetInstance(); };
	VB_LOG_TRACE("[ CmdCopy ] size: %u, offset: %u", size, device->stagingOffset);
	if (device->init_info.staging_buffer_size - device->stagingOffset < size) [[unlikely]] {
		VB_LOG_WARN("Not enough size in staging buffer to copy");
		return false;
	}
	vmaCopyMemoryToAllocation(resource->owner->vmaAllocator, data, device->staging.resource->allocation, device->stagingOffset, size);
	Copy(dst, device->staging, size, dstOfsset, device->stagingOffset);
	device->stagingOffset += size;
	return true;
}

void Command::Copy(Buffer const& dst, Buffer const& src, u32 size, u32 dstOffset, u32 srcOffset) {
	VkBufferCopy2 copyRegion{
		.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
		.pNext = nullptr,
		.srcOffset = srcOffset,
		.dstOffset = dstOffset,
		.size = size,
	};

	VkCopyBufferInfo2 copyBufferInfo{
		.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
		.srcBuffer = src.resource->handle,
		.dstBuffer = dst.resource->handle,
		.regionCount = 1,
		.pRegions = &copyRegion
	};

	vkCmdCopyBuffer2(resource->handle, &copyBufferInfo);
}

bool Command::Copy(Image const& dst, void const* data, u32 size) {
	auto device = resource->owner;
	auto GetInstance = [&cmd = *this->resource]() -> InstanceResource* { return cmd.GetInstance(); };
	VB_LOG_TRACE("[ CmdCopy ] size: %u, offset: %u", size, device->stagingOffset);
	if (device->init_info.staging_buffer_size - device->stagingOffset < size) [[unlikely]] {
		VB_LOG_WARN("Not enough size in staging buffer to copy");
		return false;
	}
	std::memcpy(device->stagingCpu + device->stagingOffset, data, size);
	Copy(dst, device->staging, device->stagingOffset);
	device->stagingOffset += size;
	return true;
}

void Command::Copy(Image const &dst, Buffer const &src, u32 srcOffset) {
	VB_ASSERT(!(dst.resource->aspect & Aspect::eDepth ||
				dst.resource->aspect & Aspect::eStencil),
			  "CmdCopy doesnt't support depth/stencil images");
	VkBufferImageCopy2 region{
		.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
		.pNext = nullptr,
		.bufferOffset = srcOffset,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.imageOffset = {0, 0, 0},
		.imageExtent = {dst.resource->extent.width, dst.resource->extent.height,
						dst.resource->extent.depth},
	};

	VkCopyBufferToImageInfo2 copyBufferToImageInfo{
		.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
		.srcBuffer = src.resource->handle,
		.dstImage = dst.resource->handle,
		.dstImageLayout = (VkImageLayout)dst.resource->layout,
		.regionCount = 1,
		.pRegions = &region
	};

	vkCmdCopyBufferToImage2(resource->handle, &copyBufferToImageInfo);
}

void Command::Copy(Buffer const &dst, Image const &src, u32 dstOffset,
				Offset3D imageOffset, Extent3D imageExtent) {
	VB_ASSERT(!(src.resource->aspect & Aspect::eDepth ||
				src.resource->aspect & Aspect::eStencil),
			"CmdCopy doesn't support depth/stencil images");
	VkBufferImageCopy2 region{
		.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
		.bufferOffset = dstOffset,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.imageOffset = {imageOffset.x, imageOffset.y, imageOffset.z},
		.imageExtent = {imageExtent.width, imageExtent.height,
						imageExtent.depth},
	};

	VkCopyImageToBufferInfo2 copyInfo{
		.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
		.srcImage = src.resource->handle,
		.srcImageLayout = (VkImageLayout)src.resource->layout,
		.dstBuffer = dst.resource->handle,
		.regionCount = 1,
		.pRegions = &region,
	};
	vkCmdCopyImageToBuffer2(resource->handle, &copyInfo);
}

void Command::Barrier(Image const& img, ImageBarrier const& barrier) {
	VkImageSubresourceRange range = {
		.aspectMask = (VkImageAspectFlags)img.resource->aspect,
		.baseMipLevel = 0,
		.levelCount = VK_REMAINING_MIP_LEVELS,
		.baseArrayLayer = 0,
		.layerCount = VK_REMAINING_ARRAY_LAYERS,
	};

	VkImageMemoryBarrier2 barrier2 = {
		.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext               = nullptr,
		.srcStageMask        = (VkPipelineStageFlags2)  barrier.memoryBarrier.srcStageMask,
		.srcAccessMask       = (VkAccessFlags2)         barrier.memoryBarrier.srcAccessMask,
		.dstStageMask        = (VkPipelineStageFlags2)  barrier.memoryBarrier.dstStageMask,
		.dstAccessMask       = (VkAccessFlags2)         barrier.memoryBarrier.dstAccessMask,
		.oldLayout           = (VkImageLayout)(barrier.oldLayout == ImageLayout::eUndefined
									? img.resource->layout
									: barrier.oldLayout),
		.newLayout           = (VkImageLayout)(barrier.newLayout == ImageLayout::eUndefined
									? img.resource->layout
									: barrier.newLayout),
		.srcQueueFamilyIndex = barrier.srcQueueFamilyIndex,
		.dstQueueFamilyIndex = barrier.dstQueueFamilyIndex,
		.image               = img.resource->handle,
		.subresourceRange    = range
	};

	VkDependencyInfo dependency = {
		.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext                    = nullptr,
		.dependencyFlags          = 0,
		.memoryBarrierCount       = 0,
		.pMemoryBarriers          = nullptr,
		.bufferMemoryBarrierCount = 0,
		.pBufferMemoryBarriers    = nullptr,
		.imageMemoryBarrierCount  = 1,
		.pImageMemoryBarriers     = &barrier2
	};

	vkCmdPipelineBarrier2(resource->handle, &dependency);
	img.resource->layout = barrier.newLayout;
}

void Command::Barrier(Buffer const& buf, BufferBarrier const& barrier) {
	VkBufferMemoryBarrier2 barrier2 {
		.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.pNext               = nullptr,
		.srcStageMask        = (VkPipelineStageFlags2) barrier.memoryBarrier.srcStageMask,
		.srcAccessMask       = (VkAccessFlags2)        barrier.memoryBarrier.srcAccessMask,
		.dstStageMask        = (VkPipelineStageFlags2) barrier.memoryBarrier.dstStageMask,
		.dstAccessMask       = (VkAccessFlags2)        barrier.memoryBarrier.dstAccessMask,
		.srcQueueFamilyIndex =                         barrier.srcQueueFamilyIndex,
		.dstQueueFamilyIndex =                         barrier.dstQueueFamilyIndex,
		.buffer              =                         buf.resource->handle,
		.offset              = (VkDeviceSize)          barrier.offset,
		.size                = (VkDeviceSize)          barrier.size
	};

	VkDependencyInfo dependency = {
		.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext                    = nullptr,
		.dependencyFlags          = 0,
		.bufferMemoryBarrierCount = 1,
		.pBufferMemoryBarriers    = &barrier2,
	};
	vkCmdPipelineBarrier2(resource->handle, &dependency);
}

void Command::Barrier(MemoryBarrier const& barrier) {
	VkMemoryBarrier2 barrier2 = {
		.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.pNext         = nullptr,
		.srcStageMask  = (VkPipelineStageFlags2) barrier.srcStageMask,
		.srcAccessMask = (VkAccessFlags2)        barrier.srcAccessMask,
		.dstStageMask  = (VkPipelineStageFlags2) barrier.dstStageMask,
		.dstAccessMask = (VkAccessFlags2)        barrier.dstAccessMask
	};
	VkDependencyInfo dependency = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &barrier2,
	};auto ff = vb::BufferUsage::eStorageBuffer | vb::BufferUsage::eTransferDst;
	vkCmdPipelineBarrier2(resource->handle, &dependency);
}

void Command::ClearColorImage(Image const& img, ClearColorValue const& color) {
	VkClearColorValue clearColor{{color.float32[0], color.float32[1], color.float32[2], color.float32[3]}};
	VkImageSubresourceRange range = {
		.aspectMask = (VkImageAspectFlags)img.resource->aspect,
		.baseMipLevel = 0,
		.levelCount = VK_REMAINING_MIP_LEVELS,
		.baseArrayLayer = 0,
		.layerCount = VK_REMAINING_ARRAY_LAYERS,
	};

	vkCmdClearColorImage(resource->handle, img.resource->handle,
		(VkImageLayout)img.resource->layout, &clearColor, 1, &range);
}

void Command::Blit(BlitInfo const& info) {
	auto dst = info.dst;
	auto src = info.src;
	auto regions = info.regions;

	ImageBlit const fullRegions[] = {{
		{{0, 0, 0}, Offset3D(dst.resource->extent)},
		{{0, 0, 0}, Offset3D(src.resource->extent)}
	}};

	if (regions.empty()) {
		regions = fullRegions;
	}

	VB_VLA(VkImageBlit2, blitRegions, regions.size());
	for (auto i = 0; auto& region: regions) {
		blitRegions[i] = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
			.pNext = nullptr,
			.srcSubresource{
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel       = 0,
				.baseArrayLayer = 0,
				.layerCount     = 1,
			},
			.srcOffsets = {VkOffset3D(region.src.offset), VkOffset3D(region.src.extent)},
			.dstSubresource{
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel       = 0,
				.baseArrayLayer = 0,
				.layerCount     = 1,
			},
			.dstOffsets = {VkOffset3D(region.dst.offset), VkOffset3D(region.dst.extent)},
		};
		++i;
	}

	VkBlitImageInfo2 blitInfo {
		.sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
		.pNext          = nullptr,
		.srcImage       = src.resource->handle,
		.srcImageLayout = (VkImageLayout)src.resource->layout,
		.dstImage       = dst.resource->handle,
		.dstImageLayout = (VkImageLayout)dst.resource->layout,
		.regionCount    = static_cast<u32>(blitRegions.size()),
		.pRegions       = blitRegions.data(),
		.filter         = VkFilter(info.filter),
	};

	vkCmdBlitImage2(resource->handle, &blitInfo);
}

void Command::BeginRendering(RenderingInfo const& info) {
	// auto& clearColor = info.clearColor;
	// auto& clearDepth = info.clearDepth;
	// auto& clearStencil = info.clearStencil;

	Rect2D renderArea = info.renderArea;
	if (renderArea == Rect2D{}) {
		if (info.colorAttachs.size() > 0) {
			renderArea.extent.width = info.colorAttachs[0].colorImage.resource->extent.width;
			renderArea.extent.height = info.colorAttachs[0].colorImage.resource->extent.height;
		} else if (info.depth.image.resource) {
			renderArea.extent.width = info.depth.image.resource->extent.width;
			renderArea.extent.height = info.depth.image.resource->extent.height;
		}
	}

	VkOffset2D offset(renderArea.offset.x, renderArea.offset.y);
	VkExtent2D extent(renderArea.extent.width, renderArea.extent.height);

	VB_VLA(VkRenderingAttachmentInfo, colorAttachInfos, info.colorAttachs.size());
	for (auto i = 0; auto& colorAttach: info.colorAttachs) {
		colorAttachInfos[i] = {
			.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView   = colorAttach.colorImage.resource->view,
			.imageLayout = VkImageLayout(colorAttach.colorImage.resource->layout),
			.loadOp      = VkAttachmentLoadOp(colorAttach.loadOp),
			.storeOp     = VkAttachmentStoreOp(colorAttach.storeOp),
			.clearValue  = *reinterpret_cast<VkClearValue const*>(&colorAttach.clearValue),
		};
		if (colorAttach.resolveImage.resource) {
			colorAttachInfos[i].resolveMode        = VK_RESOLVE_MODE_AVERAGE_BIT;
			colorAttachInfos[i].resolveImageView   = colorAttach.resolveImage.resource->view;
			colorAttachInfos[i].resolveImageLayout = VkImageLayout(colorAttach.resolveImage.resource->layout);
		}
		++i;
	}

	VkRenderingInfoKHR renderingInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
		.flags = 0,
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

	VkRenderingAttachmentInfo depthAttachInfo;
	if (info.depth.image.resource) {
		depthAttachInfo = {
			.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView   = info.depth.image.resource->view,
			.imageLayout = VkImageLayout(info.depth.image.resource->layout),
			.loadOp      = VkAttachmentLoadOp(info.depth.loadOp),
			.storeOp     = VkAttachmentStoreOp(info.depth.storeOp),
			// .storeOp = info.depth.image.resource->usage & ImageUsage::eTransientAttachment 
			//	? VK_ATTACHMENT_STORE_OP_DONT_CARE
			//	: VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue {
				.depthStencil = { info.depth.clearValue.depth, info.depth.clearValue.stencil }
			}
		};
		renderingInfo.pDepthAttachment = &depthAttachInfo;
	}
	VkRenderingAttachmentInfo stencilAttachInfo;
	if (info.stencil.image.resource) {
		stencilAttachInfo = {
			.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView   = info.stencil.image.resource->view,
			.imageLayout = VkImageLayout(info.stencil.image.resource->layout),
			.loadOp      = VkAttachmentLoadOp(info.stencil.loadOp),
			.storeOp     = VkAttachmentStoreOp(info.stencil.storeOp),
			.clearValue {
				.depthStencil = { info.stencil.clearValue.depth, info.stencil.clearValue.stencil }
			}
		};
		renderingInfo.pStencilAttachment = &stencilAttachInfo;
	}
	vkCmdBeginRendering(resource->handle, &renderingInfo);
}

void Command::SetViewport(Viewport const& viewport) {
	VkViewport vkViewport {
		.x        = viewport.x,
		.y        = viewport.y,
		.width    = viewport.width,
		.height   = viewport.height,
		.minDepth = viewport.minDepth,
		.maxDepth = viewport.maxDepth,
	};
	vkCmdSetViewport(resource->handle, 0, 1, &vkViewport);
}

void Command::SetScissor(Rect2D const& scissor) {
	VkRect2D vkScissor {
		.offset = { scissor.offset.x, scissor.offset.y },
		.extent = {
			.width  = scissor.extent.width,
			.height = scissor.extent.height
		}
	};
	vkCmdSetScissor(resource->handle, 0, 1, &vkScissor);
}

void Command::EndRendering() {
	vkCmdEndRendering(resource->handle);
}

void Command::BindPipeline(Pipeline const& pipeline) {
	vkCmdBindPipeline(resource->handle, (VkPipelineBindPoint)pipeline.resource->point, pipeline.resource->handle);
	// TODO(nm): bind only if not compatible for used descriptor sets or push constant range
	// ref: https://registry.khronos.org/vulkan/specs/1.2-extensions/html/vkspec.html#descriptorsets-compatibility
	vkCmdBindDescriptorSets(resource->handle, (VkPipelineBindPoint)pipeline.resource->point,
		pipeline.resource->layout, 0, 1, &resource->owner->descriptor.resource->set, 0, nullptr);
}

void Command::PushConstants(Pipeline const& pipeline, void const* data, u32 size) {
	vkCmdPushConstants(resource->handle, pipeline.resource->layout, VK_SHADER_STAGE_ALL, 0, size, data);
}

void Command::BindVertexBuffer(Buffer const& vertex_buffer) {
	VkDeviceSize offsets[] = { 0 };
	VkBuffer buffer;
	if (vertex_buffer.resource) {
		buffer = vertex_buffer.resource->handle;
	} else {
		buffer = VK_NULL_HANDLE;
	}
	vkCmdBindVertexBuffers(resource->handle, 0, 1, &buffer, offsets);
}

void Command::BindIndexBuffer(Buffer const& index_buffer) {
	vkCmdBindIndexBuffer(resource->handle, index_buffer.resource->handle, 0, VK_INDEX_TYPE_UINT32);
}

void Command::Draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance) {
	// VB_LOG_TRACE("CmdDraw(%u,%u,%u,%u)", vertex_count, instance_count, first_vertex, first_instance);
	vkCmdDraw(resource->handle, vertex_count, instance_count, first_vertex, first_instance);
}

void Command::DrawIndexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance) {
	// VB_LOG_TRACE("CmdDrawIndexed(%u,%u,%u,%u,%u)", index_count, instance_count, first_index, vertex_offset, first_instance);
	vkCmdDrawIndexed(resource->handle, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void Command::DrawMesh(Buffer const& vertex_buffer, Buffer const& index_buffer, u32 index_count) {
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(resource->handle, 0, 1, &vertex_buffer.resource->handle, offsets);
	vkCmdBindIndexBuffer(resource->handle, index_buffer.resource->handle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(resource->handle, index_count, 1, 0, 0, 0);
}

void Command::Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) {
	vkCmdDispatch(resource->handle, groupCountX, groupCountY, groupCountZ);
}

// vkWaitForFences + vkResetFences +
// vkResetCommandPool + vkBeginCommandBuffer
void Command::Begin() {
	vkWaitForFences(resource->owner->handle, 1, &resource->fence, VK_TRUE, UINT64_MAX);
	vkResetFences(resource->owner->handle, 1, &resource->fence);

	// ?VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT
	vkResetCommandPool(resource->owner->handle, resource->pool, 0);
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(resource->handle, &beginInfo);
}

void Command::End() {
	vkEndCommandBuffer(resource->handle);
	// resource->currentPipeline = {};
}

void Command::QueueSubmit(Queue const& queue, SubmitInfo const& info) {

	// VkCommandBufferSubmitInfo cmdInfo {
	// 	.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
	// 	.commandBuffer = GetHandle(),
	// };

	// VkSubmitInfo2 submitInfo {
	// 	.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
	// 	.waitSemaphoreInfoCount = static_cast<u32>(info.waitSemaphoreInfos.size()),
	// 	.pWaitSemaphoreInfos = reinterpret_cast<VkSemaphoreSubmitInfo const*>(info.waitSemaphoreInfos.data()),
	// 	.commandBufferInfoCount = 1,
	// 	.pCommandBufferInfos = &cmdInfo,
	// 	.signalSemaphoreInfoCount = static_cast<u32>(info.signalSemaphoreInfos.size()),
	// 	.pSignalSemaphoreInfos = reinterpret_cast<VkSemaphoreSubmitInfo const*>(info.signalSemaphoreInfos.data()),
	// };

	// auto result = vkQueueSubmit2(info.queue.resource->handle, 1, &submitInfo, resource->fence);
	// VB_CHECK_VK_RESULT(resource->owner->owner->init_info.checkVkResult, result, "Failed to submit command buffer");

	// Commented code can be faster but DRY
	queue.Submit(
		{{{.commandBuffer = GetHandle()}}},
		resource->fence, {
			.waitSemaphoreInfos = info.waitSemaphoreInfos,
			.signalSemaphoreInfos = info.signalSemaphoreInfos,
		});
}

auto Command::GetHandle() const -> vk::CommandBuffer {
	return resource->handle;
}

auto Command::GetFence() const -> Fence {
	return resource->fence;
}

void CommandResource::Create(u32 queueFamilyindex) {
	VkCommandPoolCreateInfo poolInfo {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = 0, // do not use VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
		.queueFamilyIndex = queueFamilyindex
	};

	VkCommandBufferAllocateInfo allocInfo {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	VB_VK_RESULT result = vkCreateCommandPool(owner->handle, &poolInfo, owner->owner->allocator, &pool);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create command pool!");

	allocInfo.commandPool = pool;
	result = vkAllocateCommandBuffers(owner->handle, &allocInfo, &handle);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to allocate command buffer!");

	VkFenceCreateInfo fenceInfo{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	result = vkCreateFence(owner->handle, &fenceInfo, owner->owner->allocator, &fence);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create fence!");
}

auto CommandResource::GetType() const -> char const* { return "CommandResource"; }

auto CommandResource::GetInstance() const -> InstanceResource* { return owner->owner; }

void CommandResource::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
	vkDestroyCommandPool(owner->handle, pool, owner->owner->allocator);
	vkDestroyFence(owner->handle, fence, owner->owner->allocator);
}
} // namespace VB_NAMESPACE
