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
	enum class Extension {
		kSwapchain,
		kCooperativeMatrix2,
		kPipelineLibrary,
		kGraphicsPipelineLibrary, // Implies kPipelineLibrary
	};

	enum class Feature {
		kUnusedAttachments,
	};

	// Default staging buffer size
	u32 static constexpr inline kStagingSize = 64 * 1024 * 1024;
	
	// By default device is created with one graphics queue
	// This is extracted to static member, because 
	// libc++ on linux does not support P1394R4, so we can not use 
	// std::span range constructor
	QueueInfo static constexpr inline kDefaultQueues[] = {
		{QueueFlagBits::eGraphics}
	};

	Feature static constexpr inline kDefaultFeatures[] = {
	};

	Extension static constexpr inline kDefaultOptionalExtensions[] = {
		Extension::kGraphicsPipelineLibrary,
	};

	std::span<QueueInfo const> queues = kDefaultQueues;
	u32 staging_buffer_size = kStagingSize;

	// Extensions to enable and require from physical device
	std::span<Extension const> required_extensions = {};

	// Additional extensions (may be not supported by physical device)
	std::span<Extension const> optional_extensions = kDefaultOptionalExtensions;

	// Features
	std::span<Feature const> required_features = {};

	// Optional features
	std::span<Feature const> optional_features = {};
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

	operator bool() const { return resource != nullptr; }
	
private:
	std::shared_ptr<DeviceResource> resource;
	friend Swapchain;
	friend Instance;
};

} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_DEVICE_HPP_