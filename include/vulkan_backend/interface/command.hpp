#ifndef VULKAN_BACKEND_COMMAND_HPP_
#define VULKAN_BACKEND_COMMAND_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <memory>
#include <span>
#elif defined(VB_DEV)
import std;
#endif

#include "../fwd.hpp"
#include "image.hpp"
#include "queue.hpp"

#ifdef MemoryBarrier
#undef MemoryBarrier
#endif

VB_EXPORT
namespace VB_NAMESPACE {
struct RenderingInfo {
	struct ColorAttachment {
		Image const&    colorImage;
		Image const&    resolveImage = {};
		LoadOp          loadOp       = LoadOp::eClear;
		StoreOp         storeOp      = StoreOp::eStore;
		ClearColorValue clearValue   = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
	};

	struct DepthStencilAttachment {
		Image const&           image;
		LoadOp                 loadOp     = LoadOp::eClear;
		StoreOp                storeOp    = StoreOp::eStore;
		ClearDepthStencilValue clearValue = {1.0f, 0};
	};

	std::span<ColorAttachment const> colorAttachs;
	DepthStencilAttachment const&    depth      = {.image = {}};
	DepthStencilAttachment const&    stencil    = {.image = {}};
	Rect2D const&                    renderArea = Rect2D{};  // == use size of colorAttachs[0] or depth
	u32                              layerCount = 1;
};

struct BlitInfo {
	Image const& dst;
	Image const& src;
	std::span<ImageBlit const> regions = {};
	Filter filter                      = Filter::eLinear;
};

/* ===== Synchronization 2 Barriers ===== */
struct MemoryBarrier {
	PipelineStageFlags srcStageMask  = PipelineStage::eAllCommands;
	AccessFlags        srcAccessMask = Access::eShaderWrite;
	PipelineStageFlags dstStageMask  = PipelineStage::eAllCommands;
	AccessFlags        dstAccessMask = Access::eShaderRead;
};

struct BufferBarrier {
	u32            srcQueueFamilyIndex = QueueFamilyIgnored;
	u32            dstQueueFamilyIndex = QueueFamilyIgnored;
	DeviceSize     offset              = 0;
	DeviceSize     size                = WholeSize;
	MemoryBarrier  memoryBarrier;
};

struct ImageBarrier {
	ImageLayout     newLayout           = ImageLayout::eUndefined; // == use previous layout
	ImageLayout     oldLayout           = ImageLayout::eUndefined; // == use previous layout
	u32             srcQueueFamilyIndex = QueueFamilyIgnored;
	u32             dstQueueFamilyIndex = QueueFamilyIgnored;
	MemoryBarrier memoryBarrier;
};

class Command {
public:
	bool Copy(Image  const& dst, const void*   data, u32 size);
	bool Copy(Buffer const& dst, const void*   data, u32 size, u32 dst_offset = 0);
	void Copy(Buffer const& dst, Buffer const& src,  u32 size, u32 dst_offset = 0, u32 src_offset = 0);
	void Copy(Image  const& dst, Buffer const& src,  u32 src_offset = 0);
	void Copy(Buffer const& dst, Image  const& src,  u32 dst_offset, Offset3D image_offset, Extent3D image_extent);

	void Barrier(Image const& img,  ImageBarrier  const& barrier = {});
	void Barrier(Buffer const& buf, BufferBarrier const& barrier = {});
	void Barrier(MemoryBarrier const& barrier = {});

	void Blit(BlitInfo const& info);
	void ClearColorImage(Image const& image, ClearColorValue const& color);

	void BeginRendering(RenderingInfo const& info);
	void SetViewport(Viewport const& viewport);
	void SetScissor(Rect2D const& scissor);
	void EndRendering();
	void BindPipeline(Pipeline const& pipeline);
	void PushConstants(Pipeline const& pipeline, const void* data, u32 size);

	void BindVertexBuffer(Buffer const& vertexBuffer);
	void BindIndexBuffer(Buffer const& indexBuffer);

	void Draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance);
	void DrawIndexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance);
	void DrawMesh(Buffer const& vertex_buffer, Buffer const& index_buffer, u32 index_count);

	void Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ);

	void Begin();
	void End();
	void QueueSubmit(Queue const& queue, SubmitInfo const& info = {});

	[[nodiscard]] auto GetHandle() const -> vk::CommandBuffer;
	[[nodiscard]] auto GetFence() const  -> Fence;

private:
	std::shared_ptr<CommandResource> resource;
	friend Device;
	friend Swapchain;
	friend SwapChainResource;
	friend DeviceResource;
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_COMMAND_HPP_