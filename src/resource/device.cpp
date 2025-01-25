#ifndef VB_USE_STD_MODULE
#include <algorithm>
#include <numeric>
#else
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
#endif

#include "vulkan_backend/interface/device/device.hpp"
#include "vulkan_backend/interface/instance/instance.hpp"
#include "vulkan_backend/interface/physical_device/physical_device.hpp"
#include "vulkan_backend/interface/queue/queue.hpp"
#include "vulkan_backend/log.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/vk_result.hpp"


PFN_vkSetDebugUtilsObjectNameEXT pfnVkSetDebugUtilsObjectNameEXT = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL vkSetDebugUtilsObjectNameEXT(VkDevice							 device,
															const VkDebugUtilsObjectNameInfoEXT* pNameInfo) {
	if (pfnVkSetDebugUtilsObjectNameEXT) {
		return pfnVkSetDebugUtilsObjectNameEXT(device, pNameInfo);
	}
	return VK_SUCCESS;
}

namespace VB_NAMESPACE {

Device::Device(Instance* const instance, PhysicalDevice* physical_device, DeviceInfo const& info)
	: ResourceBase(instance), physical_device(physical_device) {
	Create(info);
}

Device::~Device() { Free(); }

void Device::WaitQueue(Queue const& queue) {
	auto result = queue.waitIdle();
	VB_CHECK_VK_RESULT(result, "Failed to wait queue idle");
}

void Device::WaitIdle() {
	auto result = waitIdle();
	VB_CHECK_VK_RESULT(result, "Failed to wait device idle");
}

// auto Device::GetBindingInfo(u32 binding) const -> const vk::DescriptorSetLayoutBinding& {
// 	return descriptor.GetBindingInfo(binding);
// }

auto Device::GetMaxSamples() -> vk::SampleCountFlagBits { return physical_device->max_samples; }

auto Device::GetQueue(vk::QueueFlags flags, vk::SurfaceKHR supported_surface) -> Queue const* {
	for (auto& q : queues) {
		if ((q.flags & flags) == flags &&
			(supported_surface == vk::SurfaceKHR{} ||
			 physical_device->SupportsSurface(q.family, supported_surface))) {
			return &q;
		}
	}
	return nullptr;
}

auto Device::GetQueues() -> std::span<Queue const> { return queues; }

void Device::Create(DeviceInfo const& info) {
	enabled_extensions.reserve(info.extensions.size() + info.optional_extensions.size() + 1);
	for (auto const extension : info.extensions) {
		enabled_extensions.push_back(extension);
	}
	// Add additional optional extensions if supported
	for (auto const extension : info.optional_extensions) {
		if (physical_device->SupportsExtension(extension)) {
			enabled_extensions.push_back(extension);
		}
	}
	
	SetName(info.name.empty() ? physical_device->GetProperties2().properties.deviceName.data() : info.name);

	u32 maxQueuesInFamily = 0;
	for (auto& family : physical_device->queue_family_properties) {
		if (family.queueFamilyProperties.queueCount > maxQueuesInFamily) {
			maxQueuesInFamily = family.queueFamilyProperties.queueCount;
		}
	}

	// priority for each type of queue (1.0f for all)
	VB_VLA(float, priorities, maxQueuesInFamily);
	std::fill(priorities.begin(), priorities.end(), 1.0f);
	VB_VLA(vk::DeviceQueueCreateInfo, queue_create_infos, info.queues.size());
	std::copy(info.queues.begin(), info.queues.end(), queue_create_infos.begin());
	for (auto& queue_create_info : queue_create_infos) {
		if (queue_create_info.pQueuePriorities == nullptr) {
			queue_create_info.pQueuePriorities = priorities.data();
		}
	}

	vk::DeviceCreateInfo create_info{
		.pNext					 = &info.feature_chain,
		.queueCreateInfoCount	 = static_cast<u32>(queue_create_infos.size()),
		.pQueueCreateInfos		 = queue_create_infos.data(),
		.enabledLayerCount		 = static_cast<u32>(GetInstance().enabled_layers.size()),
		.ppEnabledLayerNames	 = GetInstance().enabled_layers.data(),
		.enabledExtensionCount	 = static_cast<u32>(enabled_extensions.size()),
		.ppEnabledExtensionNames = enabled_extensions.data(),
	};

	// VB_VK_RESULT result = physical_device->createDevice(createInfo, GetAllocator(), *this);
	auto [result, vk_device] = physical_device->createDevice(create_info, GetAllocator());
	vk::Device::operator=(vk_device);
	VB_CHECK_VK_RESULT(result, "Failed to create logical device!");
	VB_LOG_TRACE("Created logical device, name = %s",
				 physical_device->GetProperties2().properties.deviceName.data());

	for (auto& info : queue_create_infos) {
		for (u32 index = 0; index < info.queueCount; ++index) {
			queues.emplace_back(
				getQueue2({.queueFamilyIndex = info.queueFamilyIndex, .queueIndex = index}),
				*this,
				info.queueFamilyIndex,
				index,
				physical_device->queue_family_properties[info.queueFamilyIndex]
									   .queueFamilyProperties.queueFlags);
			auto& queue = queues.back();
		}
	}

	VmaVulkanFunctions vulkanFunctions		   = {};
	vulkanFunctions.vkGetInstanceProcAddr	   = &vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr		   = &vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocatorCreateInfo = {
		.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		//| VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT,
		.physicalDevice	  = *physical_device,
		.device			  = *this,
		.pVulkanFunctions = &vulkanFunctions,
		.instance		  = GetInstance(),
		.vulkanApiVersion = VK_API_VERSION_1_3,
	};

	result = vk::Result(vmaCreateAllocator(&allocatorCreateInfo, &vma_allocator));
	VB_CHECK_VK_RESULT(result, "Failed to create VmaAllocator!");

	if (owner->IsValidationEnabled()) {
		pfnVkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
			vkGetDeviceProcAddr(VkDevice(this), "vkSetDebugUtilsObjectName"));
	}
	// vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>
	// 	(vkGetDeviceProcAddr(VkDevice(this), "vkGetAccelerationStructureBuildSizesKHR"));
	// vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>
	// 	(vkGetDeviceProcAddr(VkDevice(this), "vkCreateAccelerationStructureKHR"));
	// vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>
	// 	(vkGetDeviceProcAddr(VkDevice(this), "vkCmdBuildAccelerationStructuresKHR"));
	// vkGetAccelerationStructureDeviceAddressKHR =
	// reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR> 	(vkGetDeviceProcAddr(VkDevice(this),
	// "vkGetAccelerationStructureDeviceAddressKHR")); vkDestroyAccelerationStructureKHR =
	// reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR> 	(vkGetDeviceProcAddr(VkDevice(this),
	// "vkDestroyAccelerationStructureKHR"));
}

void Device::LogWhyNotCreated(DeviceInfo const& info) const {
	VB_LOG_WARN("Could not create device with info"); // todo:: give reason
}

auto Device::CreatePipelineLayout(PipelineLayoutInfo const& info) -> vk::PipelineLayout {
	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
		.setLayoutCount			= static_cast<u32>(info.descriptor_set_layouts.size()),
		.pSetLayouts			= info.descriptor_set_layouts.data(),
		.pushConstantRangeCount = static_cast<u32>(info.push_constant_ranges.size()),
		.pPushConstantRanges	= info.push_constant_ranges.data(),
	};

	auto [result, pipelineLayout] = createPipelineLayout(pipelineLayoutInfo, GetAllocator());
	VB_CHECK_VK_RESULT(result, "Failed to create pipeline layout!");
	return pipelineLayout;
}

auto Device::GetOrCreatePipelineLayout(PipelineLayoutInfo const& info) -> vk::PipelineLayout {
	auto it = pipeline_layouts.find(info);
	if (it != pipeline_layouts.end()) [[likely]] {
		return it->second;
	}
	vk::PipelineLayout pipeline_layout = CreatePipelineLayout(info);
	pipeline_layouts.emplace(info, pipeline_layout);
	return pipeline_layout;
}

auto Device::GetOrCreateSampler(vk::SamplerCreateInfo const& info) -> vk::Sampler {
	auto it = samplers.find(info);
	if (it != samplers.end()) [[likely]] {
		return it->second;
	}

	auto [result, sampler] = createSampler(info, GetAllocator());
	VB_CHECK_VK_RESULT(result, "Failed to create texture sampler!");
	samplers.emplace(info, sampler);
	return sampler;
}

void Device::SetDebugUtilsName(vk::ObjectType objectType, void* handle, const char* name) {
	VB_VK_RESULT result = setDebugUtilsObjectNameEXT({
		.objectType   = objectType,
		.objectHandle = reinterpret_cast<u64>(handle),
		.pObjectName  = name,
	});
	VB_CHECK_VK_RESULT(result, "Failed to set debug utils object name!");
}


auto Device::GetResourceTypeName() const -> char const* { return "DeviceResource"; }

void Device::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), GetName().data());
	VB_VK_RESULT result = waitIdle();
	VB_CHECK_VK_RESULT(result, "Failed to wait device idle");

	for (auto& [_, sampler] : samplers) {
		destroySampler(sampler, GetAllocator());
	}

	for (auto& [_, pipeline_layout] : pipeline_layouts) {
		destroyPipelineLayout(pipeline_layout, GetAllocator());
	}

	vmaDestroyAllocator(vma_allocator);

	destroy(GetAllocator());
	VB_LOG_TRACE("[ Free ] Destroyed logical device");
}
}; // namespace VB_NAMESPACE
