#ifndef VULKAN_BACKEND_DEVICE_HPP_
#define VULKAN_BACKEND_DEVICE_HPP_

#ifndef VB_USE_STD_MODULE
#include <memory>
#include <span>
#elif defined(VB_DEV)
import std;
#endif

#include "../fwd.hpp"
#include "swapchain.hpp"
#include "queue.hpp"
#include "descriptor.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
struct DeviceInfo {
	// Default staging buffer size
	u32 static constexpr inline kStagingSize = 64 * 1024 * 1024;
	
	// By default device is created with one graphics queue
	// This is extracted to static member, because 
	// libc++ on linux does not support P1394R4, so we can not use 
	// std::span range constructor
	QueueInfo static constexpr inline kQueuesDefault[] = {
		{QueueFlagBits::eGraphics}
	};

	std::span<QueueInfo const> queues = kQueuesDefault;
	u32 staging_buffer_size = kStagingSize;

	// Graphics pipeline library
	bool pipeline_library       = true;
	bool link_time_optimization = true;
		
	// Features
	bool unused_attachments     = false;
};

class Device {
public:
	BindingInfo static constexpr kDefaultBindings[] = {
		{DescriptorType::eSampledImage},
		{DescriptorType::eStorageBuffer},
	};

	[[nodiscard]] auto CreateImage(ImageInfo const& info)           -> Image;
	[[nodiscard]] auto CreateBuffer(BufferInfo const& info)         -> Buffer;
	[[nodiscard]] auto CreatePipeline(PipelineInfo const& info)     -> Pipeline;
	[[nodiscard]] auto CreateSwapchain(SwapchainInfo const& info)   -> Swapchain;
	[[nodiscard]] auto CreateCommand(u32 queueFamilyindex)          -> Command;
	[[nodiscard]] auto CreateDescriptor(
		std::span<BindingInfo const> bindings = {{
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