#ifndef VULKAN_BACKEND_DEVICE_HPP_
#define VULKAN_BACKEND_DEVICE_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <memory>
#include <span>
#elif defined(VB_DEV)
import std;
#endif

#include "../fwd.hpp"
#include "swapchain.hpp"
#include "queue.hpp"

VB_EXPORT
namespace VB_NAMESPACE {

struct DeviceInfo {
	u32 static constexpr inline kStagingSize = 64 * 1024 * 1024;

	std::span<QueueInfo const> queues = {{{QueueFlagBits::eGraphics}}};
	u32 staging_buffer_size = kStagingSize;

	// Graphics pipeline library
	bool pipeline_library       = true;
	bool link_time_optimization = true;
		
	// Features
	bool unused_attachments     = false;
};

struct BindingInfo {
	u32 static constexpr kBindingAuto = ~0u;
	u32 static constexpr kMaxDescriptors = 8192;
	// Type of descriptor (e.g., DescriptorType::Sampler)
	DescriptorType const& type;
	// Binding number for this type.
	// If not specified, will be assigned automatically
	u32 binding = kBindingAuto;
	// Max number of descriptors for this binding
	u32 count = kMaxDescriptors;
	// Shader stages that can access this resource
	ShaderStage stage_flags = ShaderStage::eAll;
	// Use update after bind flag
	bool update_after_bind = false;
	bool partially_bound   = true;
};

class Device {
public:
	[[nodiscard]] auto CreateImage(ImageInfo const& info)           -> Image;
	[[nodiscard]] auto CreateBuffer(BufferInfo const& info)         -> Buffer;
	[[nodiscard]] auto CreatePipeline(PipelineInfo const& info)     -> Pipeline;
	[[nodiscard]] auto CreateSwapchain(SwapchainInfo const& info)   -> Swapchain;
	[[nodiscard]] auto CreateCommand(u32 queueFamilyindex)          -> Command;
	
	[[nodiscard]] auto CreateDescriptor(std::span<BindingInfo const> bindings = {{
		{DescriptorType::eSampledImage},
		{DescriptorType::eStorageBuffer},
	}}) -> Descriptor;

	void WaitQueue(Queue const& queue);
	void WaitIdle();

	void UseDescriptor(Descriptor const& descriptor);
	auto GetBinding(DescriptorType const type) -> u32;

	void ResetStaging();
	auto GetStagingPtr()    -> u8*;
	auto GetStagingOffset() -> u32;
	auto GetStagingSize()   -> u32;

	auto GetMaxSamples()    -> SampleCount;

	auto GetQueue(QueueInfo const& info = {}) -> Queue;
	auto GetQueues() -> std::span<Queue>;
	
private:
	std::shared_ptr<DeviceResource> resource;
	friend Swapchain;
	friend Instance;
};

} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_DEVICE_HPP_