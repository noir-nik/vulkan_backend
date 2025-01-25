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

#ifndef VB_DEV
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif

#include "vulkan_backend/classes/base.hpp"
#include "vulkan_backend/defaults/image.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/buffer/buffer.hpp"
#include "vulkan_backend/interface/command/command.hpp"
#include "vulkan_backend/interface/device/info.hpp"
#include "vulkan_backend/interface/image/image.hpp"
#include "vulkan_backend/interface/instance/instance.hpp" // allocator
#include "vulkan_backend/interface/pipeline/info.hpp"
#include "vulkan_backend/interface/pipeline_library/info.hpp"
#include "vulkan_backend/interface/swapchain/swapchain.hpp"
#include "vulkan_backend/util/hash.hpp" // PipelineLayoutInfo hash

VB_EXPORT
namespace VB_NAMESPACE {
// Owned by InstanceResource, owns other resources
class Device : public vk::Device, NoCopyNoMove, public Named, public ResourceBase<Instance> {
  public:
	Device(Instance* const instance, PhysicalDevice* physical_device, DeviceInfo const& info);
	~Device();
	// Graphics pipeline library functions (VK_EXT_graphics_pipeline_library)
	auto CreateVertexInputInterface(VertexInputInfo const& info) -> vk::Pipeline;
	auto CreatePreRasterizationShaders(PreRasterizationInfo const& info) -> vk::Pipeline;
	auto CreateFragmentShader(FragmentShaderInfo const& info) -> vk::Pipeline;
	auto CreateFragmentOutputInterface(FragmentOutputInfo const& info) -> vk::Pipeline;
	auto LinkPipeline(std::array<vk::Pipeline, 4> const& pipelines, vk::PipelineLayout layout,
					  bool link_time_optimization) -> vk::Pipeline;

	// These resources are stored in hashmap
	// Freed automatically
	[[nodiscard]] auto GetOrCreatePipelineLayout(PipelineLayoutInfo const& info) -> vk::PipelineLayout;
	[[nodiscard]] auto GetOrCreateSampler(vk::SamplerCreateInfo const& info = defaults::linearSampler)
		-> vk::Sampler;

	void WaitQueue(Queue const& queue);
	void WaitIdle();

	// Get max supported samples
	auto GetMaxSamples() -> vk::SampleCountFlagBits;
	
	// Get queue what fits the given queue info or nullptr if not found
	auto GetQueue(vk::QueueFlags flags, vk::SurfaceKHR supported_surface = nullptr) -> Queue const*;
	
	// Get all created queues
	auto GetQueues() -> std::span<Queue const>;

	// Get owning InstanceResource
	inline auto GetInstance() const -> Instance& { return *owner; }
	inline auto GetAllocator() const -> vk::AllocationCallbacks* { return owner->allocator; }
	// ResourceBase override
	auto GetResourceTypeName() const -> char const* override;
	void SetDebugUtilsName(vk::ObjectType objectType, void* handle, const char* name);

  private:
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
	// void CreateBindlessDescriptor(DescriptorInfo const& info = defaults::kBindlessDescriptorInfo);

	vk::PipelineCache  pipeline_cache			= nullptr;
	PhysicalDevice* physical_device				= nullptr;

	// must be set before creating buffers or images
	// BindlessDescriptor descriptor;

	// Created with device and not changed
	std::vector<Queue> queues;

	VmaAllocator vma_allocator;

	std::unordered_map<vk::SamplerCreateInfo, vk::Sampler>	   samplers;
	std::unordered_map<PipelineLayoutInfo, vk::PipelineLayout> pipeline_layouts;
	std::unordered_set<vk::Pipeline>						   pipelines;

	std::vector<char const*> enabled_extensions;
};
} // namespace VB_NAMESPACE
