#pragma once

#ifndef VB_USE_STD_MODULE
#include <memory>
#include <span>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#ifndef VB_DEV
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif

#include "buffer.hpp"
#include "command.hpp"
#include "image.hpp"
#include "pipeline_library.hpp"
#include "swapchain.hpp"
#include "vulkan_backend/classes/base.hpp"
#include "vulkan_backend/defaults/descriptor.hpp"
#include "vulkan_backend/defaults/image.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/descriptor.hpp"
#include "vulkan_backend/interface/info/descriptor.hpp"
#include "vulkan_backend/interface/info/device.hpp"
#include "vulkan_backend/interface/info/image.hpp"
#include "vulkan_backend/interface/info/pipeline.hpp"
#include "vulkan_backend/interface/info/queue.hpp"
#include "vulkan_backend/util/hash.hpp"
#include "vulkan_backend/interface/instance.hpp" // allocator
#include "vulkan_backend/types.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
// Owned by InstanceResource, owns other resources
class Device : public vk::Device, public Named, public ResourceBase<Instance>, public OwnerBase<Device> {
  public:
	// The size of the returned buffer may be larger than the requested size due to alignment,
	// get actual size with BufferResource::GetSize()
	[[nodiscard]] auto CreateBuffer(BufferInfo const& info) -> std::shared_ptr<Buffer>;
	[[nodiscard]] auto CreateImage(ImageInfo const& info) -> std::shared_ptr<Image>;
	[[nodiscard]] auto CreateSwapchain(SwapchainInfo const& info) -> std::shared_ptr<Swapchain>;
	[[nodiscard]] auto CreateCommand(u32 queueFamilyindex) -> std::shared_ptr<Command>;
	[[nodiscard]] auto CreatePipeline(PipelineInfo const& info) -> std::shared_ptr<Pipeline>;
	[[nodiscard]] auto CreatePipeline(GraphicsPipelineInfo const& info) -> std::shared_ptr<Pipeline>;

	// Some resources are stored in hashmap
	[[nodiscard]] auto GetOrCreatePipelineLayout(PipelineLayoutInfo const& info) -> vk::PipelineLayout;
	[[nodiscard]] auto GetOrCreateSampler(vk::SamplerCreateInfo const& info = defaults::linearSampler)
		-> vk::Sampler;

	void WaitQueue(Queue const& queue);
	void WaitIdle();

	auto GetBindingInfo(u32 binding) const -> const vk::DescriptorSetLayoutBinding&;
	inline auto GetBindlessPipelineLayout() const -> vk::PipelineLayout { return bindless_pipeline_layout; }

	void ResetStaging();
	auto GetStagingPtr() -> u8*;
	auto GetStagingOffset() -> u32;
	auto GetStagingSize() -> u32;

	auto GetMaxSamples() -> vk::SampleCountFlagBits;

	// Get queue what fits the given queue info or nullptr if not found
	auto GetQueue(QueueInfo const& info = {}) -> Queue*;
	// Get all created queues
	auto GetQueues() -> std::span<Queue>;

	// Get owning InstanceResource
	inline auto GetInstance() -> Instance* { return owner.get(); }
	inline auto GetAllocator() -> vk::AllocationCallbacks* { return owner->allocator; }
	// ResourceBase override
	auto GetResourceTypeName() const -> char const* override;

  private:
	Device(std::shared_ptr<Instance> const& instance, PhysicalDevice* physical_device,
		   DeviceInfo const& info);
	friend Swapchain;
	friend Instance;
	friend Pipeline;
	friend PipelineLibrary;
	friend Command;
	friend Buffer;
	friend Image;

	void Free() override;

	void Create(DeviceInfo const& info = {});
	void LogWhyNotCreated(DeviceInfo const& info) const;

	auto CreatePipelineLayout(PipelineLayoutInfo const& info) -> vk::PipelineLayout;
	void CreateBindlessDescriptor(DescriptorInfo const& info = defaults::kBindlessDescriptorInfo);

	vk::PipelineCache  pipeline_cache				= nullptr;
	vk::PipelineLayout bindless_pipeline_layout		= nullptr;

	std::shared_ptr<PhysicalDevice> physical_device = nullptr;

	// must be set before creating buffers or images
	BindlessDescriptor descriptor;

	// Created with device and not changed
	std::vector<Queue> queues;

	VmaAllocator vma_allocator;

	std::unordered_map<vk::SamplerCreateInfo, vk::Sampler>	   samplers;
	std::unordered_map<PipelineLayoutInfo, vk::PipelineLayout> pipeline_layouts;
	std::unordered_set<vk::Pipeline>						   pipelines;

	bool			bHasPipelineLibrary = false;
	PipelineLibrary pipeline_library;

	u8*		  staging_cpu	 = nullptr;
	u32		  staging_offset = 0;
	Buffer    staging		 = {};

	std::vector<char const*> enabled_extensions;
};
} // namespace VB_NAMESPACE
