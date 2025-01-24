#ifndef VULKAN_BACKEND_RESOURCE_DEVICE_HPP_
#define VULKAN_BACKEND_RESOURCE_DEVICE_HPP_

#ifndef VB_USE_STD_MODULE
#include <set>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include <vulkan/vulkan.h>

#ifndef VB_DEV
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif //VB_DEV

#include "vulkan_backend/interface/device.hpp"
#include "vulkan_backend/interface/image.hpp"
#include "vulkan_backend/interface/queue.hpp"
#include "vulkan_backend/interface/buffer.hpp"
#include "vulkan_backend/interface/descriptor.hpp"

#include "base.hpp"
#include "queue.hpp"
#include "pipeline_library.hpp"

namespace VB_NAMESPACE {
struct DeviceResource : ResourceBase<InstanceResource>, std::enable_shared_from_this<DeviceResource> {
	VkDevice         handle              = VK_NULL_HANDLE;
	VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;
	VkPipelineCache  pipelineCache       = VK_NULL_HANDLE;

	PhysicalDeviceResource* physicalDevice = nullptr;

	Descriptor descriptor = {};

	// All user-created resources owned by handle classes with
	// std::shared_ptr<...Resource> resource member
	std::set<ResourceBase<DeviceResource>*> resources;

	// Created with device and not changed
	std::vector<QueueResource> queuesResources;
	std::vector<Queue>         queues;
	DeviceInfo                 init_info;

	VmaAllocator vmaAllocator;

	struct SamplerHash {
		auto operator()(SamplerInfo const& info) const -> std::size_t;
		auto operator()(SamplerInfo const& lhs, SamplerInfo const& rhs) const -> bool;
	};

	std::unordered_map<SamplerInfo, VkSampler, SamplerHash, SamplerHash> samplers;

	bool bHasPipelineLibrary = false;
	PipelineLibrary pipelineLibrary;

	u8* stagingCpu = nullptr;
	u32 stagingOffset = 0;
	Buffer staging;
	
	std::vector<char const*> requiredExtensions;

	void Create(DeviceInfo const& info);
	void LogWhyNotCreated(DeviceInfo const& info) const;
	// Do not assume the size of the returned buffer,
	// it may be larger than the requested size due to alignment,
	// get it with GetSize()
	auto CreateBuffer(BufferInfo const& info) -> Buffer;
	auto CreateImage(ImageInfo const& info)   -> Image;
	auto CreatePipeline(PipelineInfo const& info)         -> Pipeline;
	auto GetOrCreateSampler(SamplerInfo const& info = {}) -> VkSampler;

	void SetDebugUtilsObjectName(VkDebugUtilsObjectNameInfoEXT const& pNameInfo);

	PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT = nullptr;
	// PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
	// PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
	// PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
	// PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
	// PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
	// PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;

	using ResourceBase::ResourceBase;

	auto GetName() const -> char const*;
	auto GetType() const -> char const* override;
	auto GetInstance() const -> InstanceResource* override;
	void Free() override;
};

} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_RESOURCE_DEVICE_HPP_