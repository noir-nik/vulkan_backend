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

#include "vulkan_backend/interface/command/command.hpp"
#include "vulkan_backend/interface/device/device.hpp"
#include "vulkan_backend/interface/buffer/buffer.hpp"
#include "vulkan_backend/interface/image/image.hpp"
#include "vulkan_backend/interface/pipeline/pipeline.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/util/enumerate.hpp"
#include "vulkan_backend/vk_result.hpp"
#include "vulkan_backend/log.hpp"


namespace VB_NAMESPACE {
Command::Command(Device& device, u32 queue_family_index) {
	VB_VK_RESULT result = Create(device, queue_family_index, true);
}

Command::Command() : vk::CommandBuffer{}, ResourceBase{} {}

auto Command::Create(Device& device, u32 queue_family_index, bool check_enabled) -> vk::Result {
	ResourceBase::SetOwner(&device);

	vk::CommandPoolCreateInfo poolInfo {
		// .flags = 0, // do not use VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
		.queueFamilyIndex = queue_family_index
	};

	vk::Result result;
	result = GetDevice().createCommandPool(&poolInfo, GetDevice().GetAllocator(), &pool);
	VB_VERIFY_VK_RESULT(result, check_enabled, "Failed to create command pool!", {});

	vk::CommandBufferAllocateInfo allocInfo {
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,
	};
	allocInfo.commandPool = pool;

	vk::CommandBuffer commandBuffer;
	result = GetDevice().allocateCommandBuffers(&allocInfo, &commandBuffer);
	VB_VERIFY_VK_RESULT(result, check_enabled, "Failed to allocate command buffer!", {});
	vk::CommandBuffer::operator=(commandBuffer);

	vk::FenceCreateInfo fence_info{
		.flags = vk::FenceCreateFlagBits::eSignaled,
	};

	std::tie(result, fence) = GetDevice().createFence(fence_info, GetDevice().GetAllocator());
	VB_VERIFY_VK_RESULT(result, check_enabled, "Failed to create fence!", {});
	return vk::Result::eSuccess;
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

Command::~Command() {
	if (vk::CommandBuffer::operator bool()) {
		Free();
	}
}

bool Command::Copy(vk::Buffer const& dst, StagingBuffer& staging, void const* data, u32 size, u32 dstOfsset) {
	VB_LOG_TRACE("[ CmdCopy ] size: %u, offset: %u", size, staging.GetOffset());
	if (staging.GetSize() - staging.GetOffset() < size) [[unlikely]] {
		VB_LOG_WARN("Not enough size in staging buffer to copy");
		return false;
	}
	vmaCopyMemoryToAllocation(GetDevice().GetVmaAllocator(), data, staging.allocation, staging.GetOffset(), size);
	Copy(dst, staging, size, dstOfsset, staging.GetOffset());
	staging.SetOffset(staging.GetOffset() + size);
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

void Command::Copy(vb::Buffer const& dst, vb::Buffer const& src) {
	VB_HOT_ASSERT(dst.GetSize() >= src.GetSize(), "Dst buffer is too small");
	vk::BufferCopy2 copyRegion{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = dst.GetSize(),
	};

	vk::CopyBufferInfo2 copyBufferInfo{
		.srcBuffer = src,
		.dstBuffer = dst,
		.regionCount = 1,
		.pRegions = &copyRegion
	};

	copyBuffer2(&copyBufferInfo);
}

bool Command::Copy(Image const& dst, StagingBuffer& staging, void const* data, u32 size) {
	VB_LOG_TRACE("[ CmdCopy ] size: %u, offset: %u", size, staging.GetOffset());
	if (staging.GetSize() - staging.GetOffset() < size) [[unlikely]] {
		VB_LOG_WARN("Not enough size in staging buffer to copy");
		return false;
	}
	std::memcpy(staging.GetPtr() + staging.GetOffset(), data, size);
	Copy(dst, staging, staging.GetOffset());
	staging.SetOffset(staging.GetOffset() + size);
	return true;
}

void Command::Copy(Image const &dst, vk::Buffer const &src, u32 srcOffset) {
	VB_ASSERT(!(dst.GetAspect() & vk::ImageAspectFlagBits::eDepth ||
				dst.GetAspect() & vk::ImageAspectFlagBits::eStencil),
			  "CmdCopy doesnt't support depth/stencil images");
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
		.imageExtent = dst.GetExtent(),
	};

	vk::CopyBufferToImageInfo2 copyBufferToImageInfo{
		.srcBuffer = src,
		.dstImage = dst,
		.dstImageLayout = dst.GetLayout(),
		.regionCount = 1,
		.pRegions = &region
	};

	copyBufferToImage2(&copyBufferToImageInfo);
}

void Command::Copy(vk::Buffer const &dst, Image const &src, u32 dstOffset,
				vk::Offset3D imageOffset, Extent3D imageExtent) {
	VB_ASSERT(!(src.GetAspect() & vk::ImageAspectFlagBits::eDepth ||
				src.GetAspect() & vk::ImageAspectFlagBits::eStencil),
			"CmdCopy doesn't support depth/stencil images");
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
		.imageExtent = src.GetExtent()
	};

	vk::CopyImageToBufferInfo2 copyInfo{
		.srcImage = src,
		.srcImageLayout = (vk::ImageLayout)src.GetLayout(),
		.dstBuffer = dst,
		.regionCount = 1,
		.pRegions = &region,
	};
	copyImageToBuffer2(&copyInfo);
}

void Command::Barrier(Image& img, ImageBarrier const& barrier) {
	vk::ImageSubresourceRange range = {
		.aspectMask = img.GetAspect(),
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
									? img.GetLayout()
									: barrier.oldLayout),
		.newLayout           = (barrier.newLayout == vk::ImageLayout::eUndefined
									? img.GetLayout()
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
	img.SetLayout(barrier.newLayout);
}

void Command::Barrier(vk::Buffer const& buf, BufferBarrier const& barrier) {
	vk::BufferMemoryBarrier2 barrier2 {
		.pNext               = nullptr,
		.srcStageMask        = barrier.memoryBarrier.srcStageMask,
		.srcAccessMask       = barrier.memoryBarrier.srcAccessMask,
		.dstStageMask        = barrier.memoryBarrier.dstStageMask,
		.dstAccessMask       = barrier.memoryBarrier.dstAccessMask,
		.srcQueueFamilyIndex = barrier.srcQueueFamilyIndex,
		.dstQueueFamilyIndex = barrier.dstQueueFamilyIndex,
		.buffer              = buf,
		.offset              = barrier.offset,
		.size                = barrier.size
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
		.aspectMask = (vk::ImageAspectFlags)img.GetAspect(),
		.baseMipLevel = 0,
		.levelCount = vk::RemainingMipLevels,
		.baseArrayLayer = 0,
		.layerCount = vk::RemainingArrayLayers,
	};

	clearColorImage(img,
		(vk::ImageLayout)img.GetLayout(), &clearColor, 1, &range);
}

void Command::Blit(BlitInfo const& info) {
	auto regions = info.regions;

	ImageBlit const fullRegions[] = {{
		{{0, 0, 0}, Offset3DFromExtent3D(info.dst.GetExtent())},
		{{0, 0, 0}, Offset3DFromExtent3D(info.src.GetExtent())}
	}};

	if (regions.empty()) {
		regions = fullRegions;
	}

	VB_VLA(vk::ImageBlit2, blitRegions, regions.size());
	for (auto i = 0; auto& region: regions) {
		blitRegions[i] = {
			.pNext = nullptr,
			.srcSubresource{
				.aspectMask     = vk::ImageAspectFlagBits::eColor,
				.mipLevel       = 0,
				.baseArrayLayer = 0,
				.layerCount     = 1,
			},
			.srcOffsets = {{region.src.offset, region.src.extent}},
			.dstSubresource{
				.aspectMask     = vk::ImageAspectFlagBits::eColor,
				.mipLevel       = 0,
				.baseArrayLayer = 0,
				.layerCount     = 1,
			},
			.dstOffsets = {{region.dst.offset, region.dst.extent}},
		};
		++i;
	}

	vk::BlitImageInfo2 blitInfo {
		.pNext          = nullptr,
		.srcImage       = info.src,
		.srcImageLayout = info.src.GetLayout(),
		.dstImage       = info.dst,
		.dstImageLayout = info.dst.GetLayout(),
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

	vk::Rect2D renderArea = info.render_area;
	if (renderArea == vk::Rect2D{}) {
		if (info.color_attachments.size() > 0) {
			renderArea.extent.width = info.color_attachments[0].color_image.GetExtent().width;
			renderArea.extent.height = info.color_attachments[0].color_image.GetExtent().height;
		} else if (info.depth.image) {
			renderArea.extent.width = info.depth.image.GetExtent().width;
			renderArea.extent.height = info.depth.image.GetExtent().height;
		}
	}

	vk::Offset2D offset(renderArea.offset.x, renderArea.offset.y);
	vk::Extent2D extent(renderArea.extent.width, renderArea.extent.height);

	VB_VLA(vk::RenderingAttachmentInfo, colorAttachInfos, info.color_attachments.size());
	for (auto [i, color_attach]: util::enumerate(info.color_attachments)) {
		colorAttachInfos[i] = {
			.imageView   = color_attach.color_image.GetView(),
			.imageLayout = color_attach.color_image.GetLayout(),
			.loadOp      = color_attach.load_op,
			.storeOp     = color_attach.store_op,
			.clearValue  = *reinterpret_cast<vk::ClearValue const*>(&color_attach.clear_value),
		};
		if (color_attach.resolve_image) {
			colorAttachInfos[i].resolveMode        = vk::ResolveModeFlagBits::eAverage;
			colorAttachInfos[i].resolveImageView   = color_attach.resolve_image.GetView();
			colorAttachInfos[i].resolveImageLayout = vk::ImageLayout(color_attach.resolve_image.GetLayout());
		}
	}

	vk::RenderingInfoKHR renderingInfo{
		.flags = {},
		.renderArea = {
			.offset = offset,
			.extent = extent,
		},
		.layerCount = info.layer_count,
		.viewMask = 0,
		.colorAttachmentCount = static_cast<u32>(colorAttachInfos.size()),
		.pColorAttachments = colorAttachInfos.data(),
		.pDepthAttachment = nullptr,
		.pStencilAttachment = nullptr,
	};

	vk::RenderingAttachmentInfo depthAttachInfo;
	if (info.depth.image) {
		depthAttachInfo = {
			.imageView   = info.depth.image.GetView(),
			.imageLayout = info.depth.image.GetLayout(),
			.loadOp      = info.depth.load_op,
			.storeOp     = info.depth.store_op,
			// .storeOp = info.depth.image.usage & vk::ImageUsageFlagBits::eTransientAttachment 
			//	? vk::AttachmentStoreOp::eDontCare
			//	: vk::AttachmentStoreOp::eStore,
			.clearValue {
				.depthStencil = { info.depth.clear_value.depth, info.depth.clear_value.stencil }
			}
		};
		renderingInfo.pDepthAttachment = &depthAttachInfo;
	}
	vk::RenderingAttachmentInfo stencilAttachInfo;
	if (info.stencil.image) {
		stencilAttachInfo = {
			.imageView   = info.stencil.image.GetView(),
			.imageLayout = info.stencil.image.GetLayout(),
			.loadOp      = info.stencil.load_op,
			.storeOp     = info.stencil.store_op,
			.clearValue {
				.depthStencil = { info.stencil.clear_value.depth, info.stencil.clear_value.stencil }
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
	bindPipeline(pipeline.GetBindPoint(), pipeline);
}

void Command::BindPipelineAndDescriptorSet(Pipeline const& pipeline, vk::DescriptorSet const& descriptor_set) {
	bindPipeline(pipeline.GetBindPoint(), pipeline);
	// TODO(nm): bind only if not compatible for used descriptor sets or push constant range
	// ref: https://registry.khronos.org/vulkan/specs/1.2-extensions/html/vkspec.html#descriptorsets-compatibility
	bindDescriptorSets(pipeline.GetBindPoint(), pipeline.GetLayout(), 0, 1, &descriptor_set, 0, nullptr);
	// Vulkan 1.4 or VK_KHR_maintenance6
	// vk::BindDescriptorSetsInfo{}
	// bindDescriptorSets2()
}

void Command::PushConstants(Pipeline const& pipeline, void const* data, u32 size) {
	pushConstants(pipeline.GetLayout(), vk::ShaderStageFlagBits::eAll, 0, size, data);
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
	result = GetDevice().waitForFences(1, &fence, vk::True, std::numeric_limits<u64>::max());
	VB_CHECK_VK_RESULT(result, "Failed to wait for fence");
	result = GetDevice().resetFences(1, &fence);
	VB_CHECK_VK_RESULT(result, "Failed to reset fence");

	// ?VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT
	result = GetDevice().resetCommandPool(pool, vk::CommandPoolResetFlags{});
	VB_CHECK_VK_RESULT(result, "Failed to reset command pool");
	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	result = begin(&beginInfo);
	VB_CHECK_VK_RESULT(result, "Failed to begin command buffer");
}

void Command::End() {
	VB_VK_RESULT result = end();
	VB_CHECK_VK_RESULT(result, "Failed to end command buffer");
}

void Command::Submit(vk::Queue const& queue, SubmitInfo const& info) {

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

auto Command::GetResourceTypeName() const -> char const* { return "CommandResource"; }

// auto Command::GetInstance() const -> Instance* { return GetDevice().owner; }

void Command::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), "Command");
	GetDevice().destroyCommandPool(pool, GetDevice().GetAllocator());
	GetDevice().destroyFence(fence, GetDevice().GetAllocator());
}
} // namespace VB_NAMESPACE
