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

#ifndef VB_USE_VMA_MODULE
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif

#include "vulkan_backend/classes/base.hpp"
#include "vulkan_backend/defaults/image.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/command/command.hpp"
#include "vulkan_backend/interface/device/info.hpp"
#include "vulkan_backend/interface/instance/instance.hpp" // allocator
#include "vulkan_backend/interface/pipeline/info.hpp"
#include "vulkan_backend/interface/pipeline_layout/info.hpp"
#include "vulkan_backend/interface/pipeline_layout/pipeline_layout.hpp"
#include "vulkan_backend/interface/pipeline_library/info.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
// Owned by InstanceResource, owns other resources
class Device : public vk::Device, NoCopyNoMove, public Named, public ResourceBase<Instance> {
  public:
	// No-op constructor
	Device();

	// RAII constructor, calls Create
	Device(Instance& instance, PhysicalDevice& physical_device, DeviceInfo const& info);

	// Create with result checked
	vk::Result Create(Instance& instance, PhysicalDevice& physical_device, DeviceInfo const& info);

	// Frees resources
	~Device();

	[[nodiscard]] auto CreateCommand(u32 queue_family_index) -> Command;
	[[nodiscard]] auto CreateDescriptor(DescriptorInfo const& info) -> Descriptor;
	[[nodiscard]] auto CreatePipelineLayout(PipelineLayoutInfo const& info) -> vk::PipelineLayout;
	// Graphics pipeline library functions (VK_EXT_graphics_pipeline_library)
	[[nodiscard("Not garbage-collected")]] auto CreateVertexInputInterface(VertexInputInfo const& info) -> vk::Pipeline;
	[[nodiscard("Not garbage-collected")]] auto CreatePreRasterizationShaders(PreRasterizationInfo const& info) -> vk::Pipeline;
	[[nodiscard("Not garbage-collected")]] auto CreateFragmentShader(FragmentShaderInfo const& info) -> vk::Pipeline;
	[[nodiscard("Not garbage-collected")]] auto CreateFragmentOutputInterface(FragmentOutputInfo const& info) -> vk::Pipeline;
	[[nodiscard("Not garbage-collected")]] auto LinkPipeline(std::array<vk::Pipeline, 4> const& pipelines,
															 vk::PipelineLayout layout, bool link_time_optimization)
		-> vk::Pipeline;

	// These resources are stored in hashmap and freed automatically
	[[nodiscard]] auto GetOrCreateSampler(vk::SamplerCreateInfo const& info = defaults::linearSampler) -> vk::Sampler;

	void WaitQueue(Queue const& queue);
	void WaitIdle();

	// Get max supported samples
	auto GetMaxSamples() -> vk::SampleCountFlagBits;

	// Get queue what fits the given queue info or nullptr if not found
	auto GetQueue(QueueInfo const& info) -> Queue const*;

	// Get all created queues
	auto GetQueues() -> std::span<Queue const>;

	// Get owning InstanceResource
	inline auto GetInstance() const -> Instance& { return *GetOwner(); }
	inline auto GetPhysicalDevice() const -> PhysicalDevice& { return *physical_device; }
	inline auto GetPipelineCache() const -> vk::PipelineCache { return pipeline_cache; }
	inline auto GetAllocator() const -> vk::AllocationCallbacks const* { return GetInstance().GetAllocator(); }
	inline auto GetVmaAllocator() -> VmaAllocator& { return vma_allocator; }
	// ResourceBase override
	auto GetResourceTypeName() const -> char const* override;
	void SetDebugUtilsName(vk::ObjectType objectType, void* handle, const char* name);

  private:
	void Free() override;

	void LogWhyNotCreated(DeviceInfo const& info) const;

	// void CreateBindlessDescriptor(DescriptorInfo const& info = defaults::kBindlessDescriptorInfo);

	vk::PipelineCache pipeline_cache  = nullptr;
	PhysicalDevice*   physical_device = nullptr;

	// Created with device and not changed
	std::vector<Queue> queues;

	VmaAllocator vma_allocator;

	// std::unordered_map<vk::SamplerCreateInfo, vk::Sampler>	   samplers;
	// std::unordered_set<vk::Pipeline>						   pipelines;

	std::vector<char const*> enabled_extensions;
};
} // namespace VB_NAMESPACE
