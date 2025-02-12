#pragma once

#ifndef VB_USE_STD_MODULE
#include <span>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/interface/command/structs.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/image/image.hpp"
#include "vulkan_backend/interface/buffer/buffer.hpp"

#ifdef MemoryBarrier
#undef MemoryBarrier
#endif

VB_EXPORT
namespace VB_NAMESPACE {
// Command handle
using CommandRef = Command*;

class Command : public vk::CommandBuffer, public ResourceBase<Device> {
public:
	// No-op constructor
	Command();
	
	// RAII constructor, calls Create
	Command(Device& device, u32 queue_family_index);

	// Create with result checked
	auto Create(Device& device, u32 queue_family_index, bool check_vk_results = true) -> vk::Result;
	
	// Create with result returned
	[[nodiscard]] vk::Result CreateWithResult(Device& device, u32 queue_family_index);
	
	// Move constructor
	Command(Command&& other);

	// Move assignment
	Command& operator=(Command&& other);

	// Destructor, frees resources
	~Command();

	bool Copy(Image      const& dst, StagingBuffer& staging, const void* data, u32 size);
	bool Copy(vk::Buffer const& dst, StagingBuffer& staging, const void* data, u32 size, u32 dst_offset = 0);
	void Copy(vk::Buffer const& dst, vk::Buffer const& src,  u32 size, u32 dst_offset = 0, u32 src_offset = 0);
	void Copy(vb::Buffer const& dst, vb::Buffer const& src);
	void Copy(Image      const& dst, vk::Buffer const& src,  u32 src_offset = 0);
	void Copy(vk::Buffer const& dst, Image      const& src,  u32 dst_offset, vk::Offset3D image_offset, Extent3D image_extent);

	void Barrier(Image& img,  ImageBarrier const& barrier = {});
	void Barrier(vk::Buffer const& buf, BufferBarrier const& barrier = {});
	void Barrier(MemoryBarrier const& barrier = {});

	void Blit(BlitInfo const& info);
	void ClearColorImage(Image const& image, vk::ClearColorValue const& color);

	void BeginRendering(RenderingInfo const& info);
	void SetViewport(Viewport const& viewport);
	void SetScissor(vk::Rect2D const& scissor);
	void EndRendering();
	void BindPipelineAndDescriptorSet(Pipeline const& pipeline, vk::DescriptorSet const& descriptor_set);
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
	void Submit(vk::Queue const& queue, SubmitInfo const& info = {});
	auto GetFence() const  -> vk::Fence;

	auto GetDevice() const -> Device& { return *GetOwner(); }
	auto GetResourceTypeName() const-> char const* override;
private:
	friend Swapchain;
	friend Device;
	void Free() override;

	vk::CommandPool pool  = nullptr;
	vk::Fence		fence = nullptr;
};
} // namespace VB_NAMESPACE

