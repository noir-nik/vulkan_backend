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

#include "vulkan_backend/compile_shader.hpp"
#include "vulkan_backend/interface/buffer.hpp"
#include "vulkan_backend/interface/command.hpp"
#include "vulkan_backend/interface/descriptor.hpp"
#include "vulkan_backend/interface/device.hpp"
#include "vulkan_backend/interface/image.hpp"
#include "vulkan_backend/interface/info/buffer.hpp"
#include "vulkan_backend/interface/instance.hpp"
#include "vulkan_backend/interface/queue.hpp"
#include "vulkan_backend/interface/swapchain.hpp"
#include "vulkan_backend/interface/pipeline.hpp"
#include "vulkan_backend/log.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/util/bits.hpp"
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

auto Device::CreateBuffer(BufferInfo const& info) -> std::shared_ptr<Buffer> {
	return std::make_shared<Buffer>(shared_from_this(), info);
}

auto Device::CreateImage(ImageInfo const& info) -> std::shared_ptr<Image> {
	return std::make_shared<Image>(shared_from_this(), info);
}

auto Device::CreateSwapchain(SwapchainInfo const& info) -> std::shared_ptr<Swapchain> {
	return std::make_shared<Swapchain>(shared_from_this(), info);
}

auto Device::CreateCommand(u32 queueFamilyindex) -> std::shared_ptr<Command> {
	return std::make_shared<Command>(shared_from_this(), queueFamilyindex);
}

auto Device::CreatePipeline(PipelineInfo const& info) -> std::shared_ptr<Pipeline> {
	return std::make_shared<Pipeline>(shared_from_this(), info);
}

auto Device::CreatePipeline(GraphicsPipelineInfo const& info) -> std::shared_ptr<Pipeline> {
	return std::make_shared<Pipeline>(shared_from_this(), info);
}

void Device::CreateBindlessDescriptor(DescriptorInfo const& info) {
	VB_ASSERT(info.bindings.size() > 0, "Descriptor must have at least one binding");
	descriptor.Create(this, info);
}

void Device::WaitQueue(Queue const& queue) {
	auto result = queue.waitIdle();
	VB_CHECK_VK_RESULT(result, "Failed to wait idle command buffer");
}

void Device::WaitIdle() {
	auto result = waitIdle();
	VB_CHECK_VK_RESULT(result, "Failed to wait idle command buffer");
}

auto Device::GetBindingInfo(u32 binding) const -> const vk::DescriptorSetLayoutBinding& {
	return descriptor.GetBindingInfo(binding);
}

void Device::ResetStaging() { staging_offset = 0; }

auto Device::GetStagingPtr() -> u8* { return staging_cpu + staging_offset; }

auto Device::GetStagingOffset() -> u32 { return staging_offset; }

auto Device::GetStagingSize() -> u32 { return staging ? staging.GetSize() : 0u; }

auto Device::GetMaxSamples() -> vk::SampleCountFlagBits { return physical_device->max_samples; }

auto Device::GetQueue(QueueInfo const& info) -> Queue* {
	for (auto& q : queues) {
		if ((q.info.flags & info.flags) == info.flags &&
			(info.supported_surface == vk::SurfaceKHR{} ||
			 physical_device->SupportsSurface(q.family, info.supported_surface))) {
			return &q;
		}
	}
	return nullptr;
}

auto Device::GetQueues() -> std::span<Queue> { return queues; }

#pragma region Resource
Device::Device(std::shared_ptr<Instance> const&		  instance,
			   PhysicalDevice* physical_device, DeviceInfo const& info)
	: ResourceBase(instance), physical_device(physical_device) {
	pipeline_library.device = this;
	SetName(info.name);
	Create(info);
}
void Device::Create(DeviceInfo const& info) {
	enabled_extensions.reserve(info.extensions.size() + info.optional_extensions.size() + 1);
	for (auto const extension : info.extensions) {
		enabled_extensions.push_back(extension);
	}
	for (auto const extension : info.optional_extensions) {
		if (physical_device->SupportsExtensions({{extension}})) {
			enabled_extensions.push_back(extension);
		}
	}

	auto ContainsExtension = [](std::span<char const*> extensions, std::string_view extension) {
		return std::find_if(extensions.begin(), extensions.end(),
							[extension](char const* availableExtension) {
								return extension == availableExtension;
							}) != extensions.end();
	};

	// Add swapchain extension if any of the queues have supported_surface != nullptr
	if (std::any_of(info.queues.begin(), info.queues.end(),
					[](QueueInfo const& q) { return bool(q.supported_surface); })) {
		if (!ContainsExtension(enabled_extensions, vk::KHRSwapchainExtensionName)) {
			enabled_extensions.push_back(vk::KHRSwapchainExtensionName);
		}
	}

	if (this->physical_device == nullptr) [[unlikely]] {
		LogWhyNotCreated(info);
		VB_ASSERT(false, "Failed to find suitable device");
		return;
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
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos =
		physical_device->GetQueueCreateInfos(info.queues);

	for (auto& queueCreateInfo : queueCreateInfos) {
		queueCreateInfo.pQueuePriorities = priorities.data();
	}

	auto ExtensionRequested = [&info](std::string_view extension) -> bool {
		return std::any_of(info.extensions.begin(), info.extensions.end(),
						   [extension](char const* e) { return e == extension; }) ||
			   std::any_of(info.optional_extensions.begin(), info.optional_extensions.end(),
						   [extension](char const* e) { return e == extension; });
	};

	// FeatureChain requiredFeatures{true};

	// vk::PhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphicsPipelineLibraryFeatures{
	// 	.graphicsPipelineLibrary = true};
	// if (ExtensionRequested(vk::EXTGraphicsPipelineLibraryExtensionName) &&
	// 	physical_device->base_features.graphics_pipeline_library.graphicsPipelineLibrary == true) {
	// 	InsertStructureAfter(info.feature_chain,
	// reinterpret_cast<vk::BaseOutStructure*>(&graphicsPipelineLibraryFeatures)); 	this->bHasPipelineLibrary =
	// true;
	// }

	auto FindPipelineLibrary = [](vk::PhysicalDeviceFeatures2 const& features2) -> bool {
		vk::BaseOutStructure* iter = reinterpret_cast<vk::BaseOutStructure*>(features2.pNext);
		while (iter != nullptr) {
			if (iter->sType == vk::StructureType::ePhysicalDeviceGraphicsPipelineLibraryFeaturesEXT) {
				auto* p = reinterpret_cast<vk::PhysicalDeviceGraphicsPipelineLibraryFeaturesEXT*>(iter);
				return p->graphicsPipelineLibrary == true;
			}
			iter = iter->pNext;
		}
		return false;
	};

	bHasPipelineLibrary = FindPipelineLibrary(physical_device->base_features.features2);

	// Add optional extensions if supported
	std::copy_if(info.optional_extensions.begin(), info.optional_extensions.end(),
				 std::back_inserter(enabled_extensions), [this](char const* extension) {
					 return physical_device->SupportsExtensions({{extension}});
				 });

	// Add additional optional extensions if supported
	for (char const* extension : info.optional_extensions) {
		if (physical_device->SupportsExtension(extension)) {
			enabled_extensions.push_back(extension);
		}
	}

	// Add pipeline library parent extension if graphics pipeline library is requested
	if (ContainsExtension(enabled_extensions, vk::EXTGraphicsPipelineLibraryExtensionName) &&
		!ContainsExtension(enabled_extensions, vk::KHRPipelineLibraryExtensionName)) {
		enabled_extensions.push_back(vk::KHRPipelineLibraryExtensionName);
	}

	vk::DeviceCreateInfo createInfo{
		.pNext					 = &info.feature_chain,
		.queueCreateInfoCount	 = static_cast<u32>(queueCreateInfos.size()),
		.pQueueCreateInfos		 = queueCreateInfos.data(),
		.enabledExtensionCount	 = static_cast<u32>(enabled_extensions.size()),
		.ppEnabledExtensionNames = enabled_extensions.data(),
	};

	// specify the required layers to the device
	createInfo.enabledLayerCount   = GetInstance()->enabled_layers.size();
	createInfo.ppEnabledLayerNames = GetInstance()->enabled_layers.data();

	// VB_VK_RESULT result = physical_device->createDevice(createInfo, GetAllocator(), *this);
	auto [result, vk_device] = physical_device->createDevice(createInfo, GetAllocator());
	vk::Device::operator=(vk_device);
	VB_CHECK_VK_RESULT(result, "Failed to create logical device!");
	VB_LOG_TRACE("Created logical device, name = %s",
				 physical_device->GetProperties2().properties.deviceName.data());

	for (auto& info : queueCreateInfos) {
		for (u32 index = 0; index < info.queueCount; ++index) {
			queues.emplace_back(
				getQueue2({.queueFamilyIndex = info.queueFamilyIndex, .queueIndex = index}), this,
				info.queueFamilyIndex, index,
				QueueInfo{.flags = physical_device->queue_family_properties[info.queueFamilyIndex]
									   .queueFamilyProperties.queueFlags});
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
		.instance		  = *GetInstance(),
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

	// Create staging buffer
	staging.Create(this, {info.staging_buffer_size, vk::BufferUsageFlagBits::eTransferSrc, Memory::CPU, "Staging Buffer"});
	staging_cpu = reinterpret_cast<u8*>(staging.allocationInfo.pMappedData);
	staging_offset = 0;

	CreateBindlessDescriptor(info.descriptor_info);
	bindless_pipeline_layout = GetOrCreatePipelineLayout({
		.descriptor_set_layouts = {{descriptor.layout}},
		.push_constant_ranges	= {{{
			  .stageFlags = vk::ShaderStageFlagBits::eAll,
			  .offset	  = 0,
			  .size		  = physical_device->GetProperties2().properties.limits.maxPushConstantsSize,
		  }}},
	});
}

void Device::LogWhyNotCreated(DeviceInfo const& info) const {
	VB_LOG_WARN("Could not create device with info"); // todo:: give reason
}

auto Device::GetOrCreatePipelineLayout(PipelineLayoutInfo const& info) -> vk::PipelineLayout {
	auto it = pipeline_layouts.find(info);
	if (it != pipeline_layouts.end()) [[likely]] {
		return it->second;
	}

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
		.setLayoutCount			= static_cast<u32>(info.descriptor_set_layouts.size()),
		.pSetLayouts			= info.descriptor_set_layouts.data(),
		.pushConstantRangeCount = static_cast<u32>(info.push_constant_ranges.size()),
		.pPushConstantRanges	= info.push_constant_ranges.data(),
	};

	auto [result, pipelineLayout] = createPipelineLayout(pipelineLayoutInfo, GetAllocator());
	VB_CHECK_VK_RESULT(result, "Failed to create pipeline layout!");

	pipeline_layouts.emplace(info, pipelineLayout);
	return pipelineLayout;
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

auto Device::GetResourceTypeName() const -> char const* { return "DeviceResource"; }

void Device::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), GetName());
	VB_VK_RESULT result = waitIdle();
	VB_CHECK_VK_RESULT(result, "Failed to wait device idle");

	pipeline_library.Free();

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
#pragma endregion
}; // namespace VB_NAMESPACE
