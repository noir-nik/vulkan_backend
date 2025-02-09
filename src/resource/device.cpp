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

#ifndef VB_USE_VMA_MODULE
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif

#include "vulkan_backend/interface/device/device.hpp"
#include "vulkan_backend/interface/instance/instance.hpp"
#include "vulkan_backend/interface/physical_device/physical_device.hpp"
#include "vulkan_backend/interface/pipeline_layout/pipeline_layout.hpp"
#include "vulkan_backend/interface/pipeline_layout/info.hpp"
#include "vulkan_backend/interface/queue/queue.hpp"
#include "vulkan_backend/log.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/vk_result.hpp"
#include "vulkan_backend/vulkan_functions.hpp"

namespace VB_NAMESPACE {

// Device::Device(DeviceInfo const& info)
// 	: ResourceBase(&instance), physical_device(&physical_device) {
// 	// VB_ASSERT(physical_device != nullptr, "Physical device must not be null to create device!");
// 	Create(info);
// }
Device::Device() : vk::Device{}, ResourceBase(nullptr), physical_device(nullptr) {};
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

auto Device::GetQueue(QueueInfo const& info) -> Queue const* {
	for (auto& q : queues) {
		if ((q.flags & info.flags) == info.flags && (q.flags & info.undesired_flags) == vk::QueueFlags{} &&
			(info.supported_surface == vk::SurfaceKHR{} || physical_device->SupportsSurface(q.family, info.supported_surface))) {
			return &q;
		}
	}
	return nullptr;
}

auto Device::GetQueues() -> std::span<Queue const> { return queues; }

Device::Device(Instance& instance, PhysicalDevice& physical_device, DeviceInfo const& info) { Create(instance, physical_device, info); }

auto Device::Create(Instance& instance, PhysicalDevice& physical_device, DeviceInfo const& info) -> vk::Result {
	ResourceBase::SetOwner(&instance);
	this->physical_device = &physical_device;
	enabled_extensions.reserve(info.extensions.size() + info.optional_extensions.size() + 1);
	for (auto const extension : info.extensions) {
		enabled_extensions.push_back(extension);
	}
	// Add additional optional extensions if supported
	for (auto const extension : info.optional_extensions) {
		if (this->physical_device->SupportsExtension(extension)) {
			enabled_extensions.push_back(extension);
		}
	}

	SetName(info.name.empty() ? GetPhysicalDevice().GetProperties().GetCore10().deviceName.data() : info.name);

	u32 maxQueuesInFamily = 0;
	for (auto& family : this->physical_device->queue_family_properties) {
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
		.pNext                   = info.features2,
		.queueCreateInfoCount    = static_cast<u32>(queue_create_infos.size()),
		.pQueueCreateInfos       = queue_create_infos.data(),
		.enabledLayerCount       = static_cast<u32>(GetInstance().enabled_layers.size()),
		.ppEnabledLayerNames     = GetInstance().enabled_layers.data(),
		.enabledExtensionCount   = static_cast<u32>(enabled_extensions.size()),
		.ppEnabledExtensionNames = enabled_extensions.data(),
	};

	VB_VK_RESULT result = this->physical_device->createDevice(&create_info, GetAllocator(), this);
	VB_VERIFY_VK_RESULT(result, info.check_vk_results, "Failed to create logical device!",
						{ vk::Device::operator=(vk::Device{}); });
	// VB_CHECK_VK_RESULT(result, "Failed to create logical device!");
	VB_LOG_TRACE("Created logical device, name = %s", this->physical_device->GetProperties().GetCore10().deviceName.data());

	u32 queue_count = std::accumulate(queue_create_infos.begin(), queue_create_infos.end(), u32(0),
									  [](u32 acc, vk::DeviceQueueCreateInfo const& info) { return acc + info.queueCount; });
	queues.reserve(queue_count);

	for (auto& info : queue_create_infos) {
		for (u32 index = 0; index < info.queueCount; ++index) {
			queues.push_back({});
			auto& queue  = queues.back();
			queue        = getQueue2({.queueFamilyIndex = info.queueFamilyIndex, .queueIndex = index});
			queue.device = this;
			queue.family = info.queueFamilyIndex;
			queue.index  = index;
			queue.flags  = this->physical_device->GetQueueFamilyProperties(info.queueFamilyIndex).queueFlags;
		}
	}

	VmaVulkanFunctions vulkanFunctions    = {};
	vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr   = &vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocatorCreateInfo = {
		.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
		//| VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT,
		.physicalDevice   = *(this->physical_device),
		.device           = *this,
		.pVulkanFunctions = &vulkanFunctions,
		.instance         = GetInstance(),
		.vulkanApiVersion = VK_API_VERSION_1_3,
	};

	auto FindDeviceAddress = [](vk::PhysicalDeviceFeatures2 const* features2) -> bool {
		vk::BaseOutStructure const* iter = reinterpret_cast<vk::BaseOutStructure const*>(features2);
		while (iter != nullptr) {
			if (iter->sType == vk::StructureType::ePhysicalDeviceVulkan12Features) {
				auto* p = reinterpret_cast<vk::PhysicalDeviceVulkan12Features const*>(iter);
				return p->bufferDeviceAddress == vk::True;
			} else if (iter->sType == vk::StructureType::ePhysicalDeviceBufferDeviceAddressFeatures) {
				auto* p = reinterpret_cast<vk::PhysicalDeviceBufferDeviceAddressFeatures const*>(iter);
				return p->bufferDeviceAddress == vk::True;
			}
			iter = iter->pNext;
		}
		return false;
	};

	if (algo::SpanContainsString(enabled_extensions, vk::KHRBufferDeviceAddressExtensionName) ||
		FindDeviceAddress(info.features2)) {
		allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	}

	result = vk::Result(vmaCreateAllocator(&allocatorCreateInfo, &vma_allocator));
	VB_VERIFY_VK_RESULT(result, info.check_vk_results, "Failed to create VmaAllocator!", {
		destroy(GetAllocator());
		vk::Device::operator=(vk::Device{});
	});

	if (GetInstance().IsValidationEnabled()) {
		LoadDeviceDebugUtilsFunctionsEXT(*this);
	}
	return vk::Result::eSuccess;
}

void Device::LogWhyNotCreated(DeviceInfo const& info) const {
	VB_LOG_WARN("Could not create device with info"); // todo:: give reason
}

auto Device::CreatePipelineLayout(PipelineLayoutInfo const& info) -> vk::PipelineLayout {
	vk::PipelineLayoutCreateInfo pipeline_layout_info{
		.setLayoutCount         = static_cast<u32>(info.descriptor_set_layouts.size()),
		.pSetLayouts            = info.descriptor_set_layouts.data(),
		.pushConstantRangeCount = static_cast<u32>(info.push_constant_ranges.size()),
		.pPushConstantRanges    = info.push_constant_ranges.data(),
	};

	vk::PipelineLayout pipeline_layout;
	VB_VK_RESULT result = createPipelineLayout(&pipeline_layout_info, GetAllocator(), &pipeline_layout);
	VB_CHECK_VK_RESULT(result, "Failed to create pipeline layout!");
	return pipeline_layout;
}

void Device::SetDebugUtilsName(vk::ObjectType objectType, void* handle, const char* name) {
	if (!GetInstance().IsValidationEnabled())
		return;
	VB_VK_RESULT result = setDebugUtilsObjectNameEXT({
		.objectType   = objectType,
		.objectHandle = reinterpret_cast<u64>(handle),
		.pObjectName  = name,
	});
	VB_CHECK_VK_RESULT(result, "Failed to set debug utils object name!");
}

auto Device::GetResourceTypeName() const -> char const* { return "DeviceResource"; }

void Device::Free() {
	if (vk::Device::operator bool()) {
		VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), GetName().data());
		VB_VK_RESULT result = waitIdle();
		VB_CHECK_VK_RESULT(result, "Failed to wait device idle");
		vmaDestroyAllocator(vma_allocator);

		destroy(GetAllocator());
		VB_LOG_TRACE("[ Free ] Destroyed logical device");
	}
}
}; // namespace VB_NAMESPACE
