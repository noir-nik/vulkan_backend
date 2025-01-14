#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <algorithm>
#include <numeric>
#else
import std;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
// import vulkan_hpp;
#endif

#include <vulkan/vulkan.h>

#include <vk_mem_alloc.h>

#include "vulkan_backend/interface/device.hpp"
#include "vulkan_backend/interface/command.hpp"
#include "device.hpp"
#include "queue.hpp"
#include "buffer.hpp"
#include "image.hpp"
#include "pipeline.hpp"
#include "swapchain.hpp"
#include "descriptor.hpp"
#include "command.hpp"
#include "instance.hpp"
#include "../util.hpp"
#include "../compile_shader.hpp"


namespace VB_NAMESPACE {
auto DeviceResource::SamplerHash::operator()(SamplerInfo const& info) const -> std::size_t {
	std::size_t hash = 0;
	hash = HashCombine(hash, static_cast<std::size_t>(info.magFilter));
	hash = HashCombine(hash, static_cast<std::size_t>(info.minFilter));
	hash = HashCombine(hash, static_cast<std::size_t>(info.mipmapMode));
	hash = HashCombine(hash, static_cast<std::size_t>(info.wrap.u));
	hash = HashCombine(hash, static_cast<std::size_t>(info.wrap.v));
	hash = HashCombine(hash, static_cast<std::size_t>(info.wrap.w));
	hash = HashCombine(hash, std::hash<float>()(info.mipLodBias));
	hash = HashCombine(hash, std::hash<bool>()(info.anisotropyEnable));
	hash = HashCombine(hash, std::hash<float>()(info.maxAnisotropy));
	hash = HashCombine(hash, std::hash<bool>()(info.compareEnable));
	hash = HashCombine(hash, static_cast<std::size_t>(info.compareOp));
	hash = HashCombine(hash, std::hash<float>()(info.minLod));
	hash = HashCombine(hash, std::hash<float>()(info.maxLod));
	hash = HashCombine(hash, static_cast<std::size_t>(info.borderColor));
	hash = HashCombine(hash, std::hash<bool>()(info.unnormalizedCoordinates));
	return hash;
}

auto DeviceResource::SamplerHash::operator()(SamplerInfo const& lhs, SamplerInfo const& rhs) const -> bool {
	return std::memcmp(&lhs, &rhs, sizeof(SamplerInfo)) == 0;
}

auto Device::CreateBuffer(BufferInfo const& info) -> Buffer {
	return resource->CreateBuffer(info);
}

auto Device::CreateImage(ImageInfo const& info) -> Image {
	return resource->CreateImage(info);
}

auto Device::CreatePipeline(PipelineInfo const& info) -> Pipeline {
	VB_ASSERT(info.stages.size() > 0, "Pipeline should have at least one stage!");
	if (info.point == PipelinePoint::eGraphics &&
			resource->init_info.pipeline_library &&
				resource->physicalDevice->graphicsPipelineLibraryFeatures.graphicsPipelineLibrary) {
		return resource->pipelineLibrary.CreatePipeline(info);
	}
	return resource->CreatePipeline(info);
}

auto Device::CreateSwapchain(SwapchainInfo const& info) -> Swapchain {
	Swapchain swapchain;
	swapchain.resource = MakeResource<SwapChainResource>(resource.get(), "Swapchain");
	swapchain.resource->Create(info);
	return swapchain;
}

auto Device::CreateCommand(u32 queueFamilyindex) -> Command {
	Command command;
	command.resource = MakeResource<CommandResource>(resource.get(), "Command Buffer");
	command.resource->Create(queueFamilyindex);
	return command;
}

auto Device::CreateDescriptor(std::span<BindingInfo const> bindings) -> Descriptor {
	VB_ASSERT(bindings.size() > 0, "Descriptor must have at least one binding");
	Descriptor descriptor;
	descriptor.resource = MakeResource<DescriptorResource>(resource.get(), "Descriptor");
	descriptor.resource->Create(bindings);
	return descriptor;
}

void Device::WaitQueue(Queue const& queue) {
	auto result = vkQueueWaitIdle(queue.resource->handle);
	VB_CHECK_VK_RESULT(resource->owner->init_info.checkVkResult, result, "Failed to wait idle command buffer");
}

void Device::WaitIdle() {
	auto result = vkDeviceWaitIdle(resource->handle);
	VB_CHECK_VK_RESULT(resource->owner->init_info.checkVkResult, result, "Failed to wait idle command buffer");
}

void Device::UseDescriptor(Descriptor const& descriptor) {
	resource->descriptor = descriptor;
}

auto Device::GetBinding(DescriptorType const type) -> u32 {
	return resource->descriptor.GetBinding(type);
}

void Device::ResetStaging() {
	resource->stagingOffset = 0;
}

auto Device::GetStagingPtr() -> u8* {
	return resource->stagingCpu + resource->stagingOffset;
}

auto Device::GetStagingOffset() -> u32 {
	return resource->stagingOffset;
}

auto Device::GetStagingSize() -> u32 {
	return resource->init_info.staging_buffer_size;
}

auto Device::GetMaxSamples() -> SampleCount {
	return resource->physicalDevice->maxSamples;
}

auto Device::GetQueue(QueueInfo const& info) -> Queue {
	for (auto& q : resource->queuesResources) {
		if ((q.init_info.flags & info.flags) == info.flags &&
				(bool(info.supported_surface) || 
					q.init_info.supported_surface == info.supported_surface)) {
			Queue queue;
			queue.resource = &q;
			return queue;
		}
	}
	return Queue{};
}

auto Device::GetQueues() -> std::span<Queue> {
	return resource->queues;
}

#pragma region Resource
void DeviceResource::Create(DeviceInfo const& info) {
	init_info = info;
	pipelineLibrary.device = this;
	// <family index, queue count>
	// VB_VLA(u32, numQueuesToCreate, physicalDevice->availableFamilies.size());
	std::vector<u32> numQueuesToCreate;
	// std::fill(numQueuesToCreate.begin(), numQueuesToCreate.end(), 0);

	// Add swapchain extension
		if (std::any_of( info.queues.begin(), info.queues.end(),
				[](QueueInfo const &q) { return bool(q.supported_surface); })) {
			requiredExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		}

		for (auto& physicalDevice : owner->physicalDevices) {
			// Require extension support
			if (!physicalDevice.SupportsExtensions(requiredExtensions)) {
				continue;
			}
			numQueuesToCreate.resize(physicalDevice.availableFamilies.size(), 0);
			// std::fill(numQueuesToCreate.begin(), numQueuesToCreate.end(), 0);

			bool deviceSuitable = true;
			for (auto& queue_info: info.queues) {
				// Queue choosing heuristics
				using AvoidInfo = PhysicalDeviceResource::QueueFamilyIndexRequest::AvoidInfo;
				std::span<AvoidInfo const> avoid_if_possible;
				if(queue_info.separate_family) {
					switch (QueueFlags::MaskType(queue_info.flags)) {
					case VK_QUEUE_COMPUTE_BIT: 
						avoid_if_possible = PhysicalDeviceResource::QueueFamilyIndexRequest::kAvoidCompute;
						break;
					case VK_QUEUE_TRANSFER_BIT:
						avoid_if_possible = PhysicalDeviceResource::QueueFamilyIndexRequest::kAvoidTransfer;
						break;
					default:
						avoid_if_possible = PhysicalDeviceResource::QueueFamilyIndexRequest::kAvoidOther;
						break;
					}
				}

				// Get family index fitting requirements
				PhysicalDeviceResource::QueueFamilyIndexRequest request{
					.desiredFlags = VkQueueFlags(queue_info.flags),
					.undesiredFlags = 0,
					.avoidIfPossible = avoid_if_possible,			
					.numTakenQueues = numQueuesToCreate
				};

				if (queue_info.supported_surface) {
					request.surfaceToSupport = queue_info.supported_surface;
				}

				auto familyIndex = physicalDevice.GetQueueFamilyIndex(request);
				if (familyIndex == PhysicalDeviceResource::QUEUE_NOT_FOUND) {
					VB_LOG_WARN("Requested queue flags %d not available on device: %s",
						QueueFlags::MaskType(queue_info.flags), physicalDevice.physicalProperties2.properties.deviceName);
					deviceSuitable = false;
					break;
				} else if (familyIndex == PhysicalDeviceResource::ALL_SUITABLE_QUEUES_TAKEN) {
					VB_LOG_WARN("Requested more queues with flags %d than available on device: %s. Queue was not created",
						QueueFlags::MaskType(queue_info.flags), physicalDevice.physicalProperties2.properties.deviceName);
					continue;
				}
				// Create queue
				numQueuesToCreate[familyIndex]++;
			}
		if (deviceSuitable) {
			this->physicalDevice = &physicalDevice;
			break;
		}
	}

	// if (this->physicalDevice == nullptr) {
	// 	VB_LOG_ERROR("Failed to find suitable device");
	// 	// return;
	// }
	VB_ASSERT(this->physicalDevice != nullptr, "Failed to find suitable device");

	name += physicalDevice->physicalProperties2.properties.deviceName;

	u32 maxQueuesInFamily = 0;
	for (auto& family: physicalDevice->availableFamilies) {
		if (family.queueCount > maxQueuesInFamily) {
			maxQueuesInFamily = family.queueCount;
		}
	}

	// priority for each type of queue (1.0f for all)
	VB_VLA(float, priorities, maxQueuesInFamily);
	std::fill(priorities.begin(), priorities.end(), 1.0f);

	int numFamilies = std::count_if(numQueuesToCreate.begin(), numQueuesToCreate.end(), [](int queueCount) {
		return queueCount > 0;
    });

	VB_VLA(VkDeviceQueueCreateInfo, queueCreateInfos, numFamilies);
	for (u32 queueInfoIndex{}, familyIndex{}; familyIndex < numQueuesToCreate.size(); ++familyIndex) {
		if (numQueuesToCreate[familyIndex] == 0) continue;
		queueCreateInfos[queueInfoIndex] = {
			.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = familyIndex,
			.queueCount       = numQueuesToCreate[familyIndex],
			.pQueuePriorities = priorities.data(),
		};
		++queueInfoIndex;
	}

	auto& supportedFeatures = physicalDevice->physicalFeatures2.features;
	VkPhysicalDeviceFeatures2 features2 {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.features = {
			.geometryShader    = supportedFeatures.geometryShader,
			.sampleRateShading = supportedFeatures.sampleRateShading,
			.logicOp           = supportedFeatures.logicOp,
			.depthClamp        = supportedFeatures.depthClamp,
			.fillModeNonSolid  = supportedFeatures.fillModeNonSolid,
			.wideLines         = supportedFeatures.wideLines,
			.multiViewport     = supportedFeatures.multiViewport,
			.samplerAnisotropy = supportedFeatures.samplerAnisotropy,
		}
	};

	// request features if available
	VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphicsPipelineLibraryFeatures;
	if (init_info.pipeline_library && physicalDevice->graphicsPipelineLibraryFeatures.graphicsPipelineLibrary) {
		graphicsPipelineLibraryFeatures = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT,
			.pNext = features2.pNext,
			.graphicsPipelineLibrary = true,
		}; features2.pNext = &graphicsPipelineLibraryFeatures;
		requiredExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
		requiredExtensions.push_back(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
	}

	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
		.pNext = features2.pNext,
		.shaderUniformBufferArrayNonUniformIndexing   = physicalDevice->indexingFeatures.shaderUniformBufferArrayNonUniformIndexing,
		.shaderSampledImageArrayNonUniformIndexing    = physicalDevice->indexingFeatures.shaderSampledImageArrayNonUniformIndexing,
		.shaderStorageBufferArrayNonUniformIndexing   = physicalDevice->indexingFeatures.shaderStorageBufferArrayNonUniformIndexing,
		.descriptorBindingSampledImageUpdateAfterBind = physicalDevice->indexingFeatures.descriptorBindingSampledImageUpdateAfterBind,
		.descriptorBindingStorageImageUpdateAfterBind = physicalDevice->indexingFeatures.descriptorBindingStorageImageUpdateAfterBind,
		.descriptorBindingPartiallyBound              = physicalDevice->indexingFeatures.descriptorBindingPartiallyBound,
		.runtimeDescriptorArray                       = physicalDevice->indexingFeatures.runtimeDescriptorArray,
	}; features2.pNext = &descriptorIndexingFeatures;

	VkPhysicalDeviceBufferDeviceAddressFeatures addresFeatures{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
		.pNext = features2.pNext,
		.bufferDeviceAddress = physicalDevice->bufferDeviceAddressFeatures.bufferDeviceAddress,
	}; features2.pNext = &addresFeatures;

	// VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
	// rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	// rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
	// rayTracingPipelineFeatures.pNext = &addresFeatures;

	// VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
	// accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	// accelerationStructureFeatures.accelerationStructure = VK_TRUE;
	// accelerationStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;
	// accelerationStructureFeatures.accelerationStructureCaptureReplay = VK_TRUE;
	// accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;

	// VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
	// rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
	// rayQueryFeatures.rayQuery = VK_TRUE;
	// // rayQueryFeatures.pNext = &accelerationStructureFeatures;
	// rayQueryFeatures.pNext = &addresFeatures;



	VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
		.pNext = features2.pNext,
		.dynamicRendering = physicalDevice->dynamicRenderingFeatures.dynamicRendering,
	}; features2.pNext = &dynamicRenderingFeatures;

	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT unusedAttachmentFeatures;
	if (init_info.unused_attachments && physicalDevice->unusedAttachmentFeatures.dynamicRenderingUnusedAttachments) {
		unusedAttachmentFeatures = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT,
			.pNext = features2.pNext,
			.dynamicRenderingUnusedAttachments = true,
		}; features2.pNext = &unusedAttachmentFeatures;
		requiredExtensions.push_back(VK_EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_EXTENSION_NAME);
	}

	VkPhysicalDeviceSynchronization2FeaturesKHR sync2Features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
		.pNext = features2.pNext,
		.synchronization2 = physicalDevice->synchronization2Features.synchronization2,
	}; features2.pNext = &sync2Features;

	// VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicFeatures{};
	// atomicFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
	// atomicFeatures.shaderBufferFloat32AtomicAdd = VK_TRUE;
	// atomicFeatures.pNext = &sync2Features;

	VkDeviceCreateInfo createInfo{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &features2,
		.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size()),
		.pQueueCreateInfos = queueCreateInfos.data(),
		.enabledExtensionCount = static_cast<u32>(requiredExtensions.size()),
		.ppEnabledExtensionNames = requiredExtensions.data(),
	};

	// specify the required layers to the device
	if (owner->init_info.validation_layers) {
		auto& layers = owner->activeLayersNames;
		createInfo.enabledLayerCount = layers.size();
		createInfo.ppEnabledLayerNames = layers.data();
	}

	VB_VK_RESULT result = vkCreateDevice(physicalDevice->handle, &createInfo, owner->allocator, &handle);
	VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create logical device!");
	VB_LOG_TRACE("Created logical device, name = %s", physicalDevice->physicalProperties2.properties.deviceName);

	queuesResources.reserve(std::reduce(numQueuesToCreate.begin(), numQueuesToCreate.end()));
	for (auto& info: queueCreateInfos) {
		for (u32 index = 0; index < info.queueCount; ++index) {
			queuesResources.emplace_back(QueueResource{
				.device = this,
				.familyIndex = info.queueFamilyIndex,
				.index = index,
				.init_info = {
					.flags = static_cast<QueueFlags>(physicalDevice->availableFamilies[info.queueFamilyIndex].queueFlags),
				},
			});
			auto& queue = queuesResources.back();
			vkGetDeviceQueue(handle, info.queueFamilyIndex, index, &queue.handle);
		}
	}

	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocatorCreateInfo = {
		.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT
				| VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
				//| VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT,
		.physicalDevice = physicalDevice->handle,
		.device = handle,
		.pVulkanFunctions = &vulkanFunctions,
		.instance = owner->handle,
		.vulkanApiVersion = VK_API_VERSION_1_3,
	};

	result = vmaCreateAllocator(&allocatorCreateInfo, &vmaAllocator);
	VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create allocator!");

	if (owner->init_info.validation_layers) {
		vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>
			(vkGetDeviceProcAddr(handle, "vkSetDebugUtilsObjectNameEXT"));
	}
	// vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>
	// 	(vkGetDeviceProcAddr(handle, "vkGetAccelerationStructureBuildSizesKHR"));
	// vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>
	// 	(vkGetDeviceProcAddr(handle, "vkCreateAccelerationStructureKHR"));
	// vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>
	// 	(vkGetDeviceProcAddr(handle, "vkCmdBuildAccelerationStructuresKHR"));
	// vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>
	// 	(vkGetDeviceProcAddr(handle, "vkGetAccelerationStructureDeviceAddressKHR"));
	// vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>
	// 	(vkGetDeviceProcAddr(handle, "vkDestroyAccelerationStructureKHR"));

	VkDescriptorPoolSize imguiPoolSizes[] = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000} 
	};

	VkDescriptorPoolCreateInfo imguiPoolInfo = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets       = 1024u,
		.poolSizeCount = std::size(imguiPoolSizes),
		.pPoolSizes    = imguiPoolSizes,
	};

	result = vkCreateDescriptorPool(handle, &imguiPoolInfo, owner->allocator, &imguiDescriptorPool);
	VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create imgui descriptor pool!");
	
	// Create command buffers
	// for (auto& q: queuesResources) {
	// 	q.command.resource = MakeResource<CommandResource>(this, "Command Buffer");
	// 	q.command.resource->Create(&q);
	// }

	// Create staging buffer
	staging = CreateBuffer({
		init_info.staging_buffer_size,
		BufferUsage::eTransferSrc,
		Memory::CPU,
		"Staging Buffer"
	});
	stagingCpu = reinterpret_cast<u8*>(staging.resource->allocationInfo.pMappedData);
	stagingOffset = 0;
}

auto DeviceResource::CreateBuffer(BufferInfo const& info) -> Buffer {
	BufferUsageFlags usage = info.usage;
	u32 size = info.size;

	if (usage & BufferUsage::eVertexBuffer) {
		usage |= BufferUsage::eTransferDst;
	}

	if (usage & BufferUsage::eIndexBuffer) {
		usage |= BufferUsage::eTransferDst;
	}

	if (usage & BufferUsage::eStorageBuffer) {
		usage |= BufferUsage::eShaderDeviceAddress;
		size += size % physicalDevice->physicalProperties2.properties.limits.minStorageBufferOffsetAlignment;
	}

	if (usage & BufferUsage::eAccelerationStructureBuildInputReadOnlyKHR) {
		usage |= BufferUsage::eShaderDeviceAddress;
		usage |= BufferUsage::eTransferDst;
	}

	if (usage & BufferUsage::eAccelerationStructureStorageKHR) {
		usage |= BufferUsage::eShaderDeviceAddress;
	}

	auto res = MakeResource<BufferResource>(this, info.name);
	Buffer buffer;
	buffer.resource = res;
	res->size   = size;
	res->usage  = usage;
	res->memory = info.memory;
	
	VkBufferCreateInfo bufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VkBufferUsageFlags(usage),
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateFlags constexpr cpuFlags = {
		VMA_ALLOCATION_CREATE_MAPPED_BIT |
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
	};

	VmaAllocationCreateInfo allocInfo = {
		.flags = info.memory & Memory::CPU ? cpuFlags: 0,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	VB_LOG_TRACE("[ vmaCreateBuffer ] name = %s, size = %zu", info.name.data(), bufferInfo.size);
	VB_VK_RESULT result = vmaCreateBuffer(vmaAllocator, &bufferInfo, &allocInfo, &res->handle, &res->allocation, &res->allocationInfo);
	VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create buffer!");

	// Update bindless descriptor
	if (usage & BufferUsage::eStorageBuffer) {
		VB_ASSERT(descriptor.resource != nullptr, "Descriptor resource not set!");
		res->rid = descriptor.resource->PopID(DescriptorType::eStorageBuffer);

		VkDescriptorBufferInfo bufferInfo = {
			.buffer = res->handle,
			.offset = 0,
			.range  = size,
		};

		VkWriteDescriptorSet write = {
			.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = descriptor.resource->set,
			.dstBinding      = descriptor.resource->GetBinding(DescriptorType::eStorageBuffer),
			.dstArrayElement = buffer.GetResourceID(),
			.descriptorCount = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo     = &bufferInfo,
		};

		vkUpdateDescriptorSets(handle, 1, &write, 0, nullptr);
	}
	return buffer;
}

auto DeviceResource::CreateImage(ImageInfo const& info) -> Image {
	Image image;
	image.resource = MakeResource<ImageResource>(this, info.name);
	auto& res    = image.resource;
	res->samples = info.samples;
	res->extent  = info.extent;
	res->usage   = info.usage;
	res->format  = info.format;
	res->layout  = ImageLayout::eUndefined;
	res->layers  = info.layers;

	VkImageCreateInfo imageInfo{
		.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags         = (VkImageCreateFlags)(info.layers == 6? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT: 0),
		.imageType     = VK_IMAGE_TYPE_2D,
		.format        = VkFormat(info.format),
		.extent = {
			.width     = info.extent.width,
			.height    = info.extent.height,
			.depth     = info.extent.depth,
		},
		.mipLevels     = 1,
		.arrayLayers   = info.layers,
		.samples       = (VkSampleCountFlagBits)std::min(info.samples, physicalDevice->maxSamples),
		.tiling        = VK_IMAGE_TILING_OPTIMAL,
		.usage         = (VkImageUsageFlags)info.usage,
		.sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VmaAllocationCreateInfo allocInfo = {
		.usage = VMA_MEMORY_USAGE_AUTO,
		.preferredFlags = VkMemoryPropertyFlags(info.usage & ImageUsage::eTransientAttachment
			? VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
			: 0),
	};

	VB_LOG_TRACE("[ vmaCreateImage ] name = %s, extent = %ux%ux%u, layers = %u",
		info.name.data(), info.extent.width, info.extent.height, info.extent.depth, info.layers);
	VB_VK_RESULT result = vmaCreateImage(vmaAllocator, &imageInfo, &allocInfo, &res->handle, &res->allocation, nullptr);
	VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create image!");
	
	ImageAspectFlags aspect{};
	switch (info.format) {
		case Format::eD24UnormS8Uint:
			aspect = Aspect::eStencil; // Invalid, cannot be both stencil and depth, todo: create separate imageview
		[[fallthrough]];
		case Format::eD32Sfloat:
		case Format::eD16Unorm:
			aspect |= Aspect::eDepth;
			break;
		[[likely]]
		default:
			aspect = Aspect::eColor;
			break;
	}
	res->aspect = aspect;

	VkImageViewCreateInfo viewInfo{
		.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image    = res->handle,
		.viewType = info.layers == 1? VK_IMAGE_VIEW_TYPE_2D: VK_IMAGE_VIEW_TYPE_CUBE,
		.format   = (VkFormat)info.format,
		.subresourceRange {
			.aspectMask     = VkImageAspectFlags(aspect),
			.baseMipLevel   = 0,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = info.layers
		}
	};

	// TODO(nm): Create image view only if usage if Sampled or Storage or other fitting
	result = vkCreateImageView(handle, &viewInfo, owner->allocator, &res->view);
	VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create image view!");

	if (info.layers > 1) {
		viewInfo.subresourceRange.layerCount = 1;
		res->layersView.resize(info.layers);
		for (u32 i = 0; i < info.layers; i++) {
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.subresourceRange.baseArrayLayer = i;
			result = vkCreateImageView(handle, &viewInfo, owner->allocator, &res->layersView[i]);
			VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create image view!");
		}
	}
	if (info.usage & ImageUsage::eSampled) {
		VB_ASSERT(descriptor.resource != nullptr, "Descriptor resource not set!");
		res->rid = descriptor.resource->PopID(DescriptorType::eCombinedImageSampler);
	}
	if (info.usage & ImageUsage::eStorage) {
		VB_ASSERT(descriptor.resource != nullptr, "Descriptor resource not set!");
		res->rid = descriptor.resource->PopID(DescriptorType::eStorageImage);
	}
	if (info.usage & ImageUsage::eSampled) {
		ImageLayout newLayout = ImageLayout::eShaderReadOnlyOptimal;

		ImageAspectFlags ds = Aspect::eDepth | Aspect::eStencil;
		
		if ((aspect & Aspect::eDepth) == Aspect::eDepth) [[unlikely]] {
			newLayout = ImageLayout::eDepthReadOnlyOptimal;
		}
		
		if ((aspect & ds) == ds) [[unlikely]] {
			newLayout = ImageLayout::eDepthStencilReadOnlyOptimal;
		}

		// res->imguiRIDs.resize(desc.layers);
		// if (desc.layers > 1) {
		//     for (int i = 0; i < desc.layers; i++) {
		//         res->imguiRIDs[i] = (ImTextureID)ImGui_ImplVulkan_AddTexture(, res->layersView[i], (VkImageLayout)newLayout);
		//     }
		// } else {
		//     res->imguiRIDs[0] = (ImTextureID)ImGui_ImplVulkan_AddTexture(, res->view, (VkImageLayout)newLayout);
		// }

		VkDescriptorImageInfo descriptorInfo = {
			.sampler     = GetOrCreateSampler(info.sampler),
			.imageView   = res->view,
			.imageLayout = (VkImageLayout)newLayout,
		};

		VkWriteDescriptorSet write = {
			.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = descriptor.resource->set,
			.dstBinding      = descriptor.resource->GetBinding(DescriptorType::eCombinedImageSampler),
			.dstArrayElement = image.GetResourceID(),
			.descriptorCount = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo      = &descriptorInfo,
		};

		vkUpdateDescriptorSets(handle, 1, &write, 0, nullptr);
	}
	if (info.usage & ImageUsage::eStorage) {
		VkDescriptorImageInfo descriptorInfo = {
			.sampler     = GetOrCreateSampler(info.sampler), // todo: remove
			.imageView   = res->view,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		};
		VkWriteDescriptorSet write = {
			.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = descriptor.resource->set,
			.dstBinding      = descriptor.resource->GetBinding(DescriptorType::eStorageImage),
			.dstArrayElement = image.GetResourceID(),
			.descriptorCount = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.pImageInfo      = &descriptorInfo,
		};

		vkUpdateDescriptorSets(handle, 1, &write, 0, nullptr);
	}

	SetDebugUtilsObjectName({
		.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.objectType   = VkObjectType::VK_OBJECT_TYPE_IMAGE,
		.objectHandle = reinterpret_cast<uint64_t>(res->handle),
		.pObjectName  = info.name.data(),
	});

	SetDebugUtilsObjectName({
		.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.objectType   = VkObjectType::VK_OBJECT_TYPE_IMAGE_VIEW,
		.objectHandle = reinterpret_cast<uint64_t>(res->view),
		.pObjectName  = (std::string(info.name) + "View").data(),
	});
	return image;
}

auto DeviceResource::CreatePipeline(PipelineInfo const& info) -> Pipeline {
	Pipeline pipeline;
	pipeline.resource = MakeResource<PipelineResource>(this, info.name);
	pipeline.resource->point = info.point;
	pipeline.resource->CreateLayout({{this->descriptor.resource->layout}});

	VB_VLA(VkPipelineShaderStageCreateInfo, shader_stages, info.stages.size());
	VB_VLA(VkShaderModule, shader_modules, info.stages.size());
	for (auto i = 0; auto& stage: info.stages) {
		std::vector<char> bytes = LoadShader(stage);
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = bytes.size();
		createInfo.pCode = (const u32*)(bytes.data());
		VB_VK_RESULT result = vkCreateShaderModule(handle, &createInfo, owner->allocator, &shader_modules[i]);
		VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create shader module!");
		shader_stages[i] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = (VkShaderStageFlagBits)stage.stage,
			.module = shader_modules[i],
			.pName = stage.entry_point.data(),
			.pSpecializationInfo = nullptr,
		};
		++i;
	}

	if (info.point == PipelinePoint::eCompute) {
		VB_ASSERT(shader_stages.size() == 1, "Compute pipeline supports only 1 stage.");
		VkComputePipelineCreateInfo pipelineInfo {
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.stage = shader_stages[0],
			.layout = pipeline.resource->layout,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = -1,
		};
		VB_VK_RESULT result = vkCreateComputePipelines(handle, VK_NULL_HANDLE, 1,
			&pipelineInfo, owner->allocator, &pipeline.resource->handle);
		VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create compute pipeline!");
	} else {
		// graphics pipeline
		VkPipelineRasterizationStateCreateInfo rasterizationState = {
			.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable        = VK_FALSE, // fragments beyond near and far planes are clamped to them
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode             = VK_POLYGON_MODE_FILL,
			.cullMode                = (VkCullModeFlags)info.cullMode,
			.frontFace               = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasEnable         = VK_FALSE,
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp          = 0.0f,
			.depthBiasSlopeFactor    = 0.0f,
			.lineWidth               = 1.0f, // line thickness in terms of number of fragments
		};

		VkPipelineMultisampleStateCreateInfo multisampleState = {
			.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples  = (VkSampleCountFlagBits)std::min(info.samples, physicalDevice->maxSamples),
			.sampleShadingEnable   = VK_FALSE,
			.minSampleShading      = 0.5f,
			.pSampleMask           = nullptr,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable      = VK_FALSE,
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilState = {
			.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable       = VK_TRUE,
			.depthWriteEnable      = VK_TRUE,
			.depthCompareOp        = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable     = VK_FALSE,
			.front                 = {},
			.back                  = {},
			.minDepthBounds        = 0.0f,
			.maxDepthBounds        = 1.0f,
		};

		// We use std::vector, because vertexAttributes can be empty and vla with size 0 is undefined behavior
		std::vector<VkVertexInputAttributeDescription> attributeDescs(info.vertexAttributes.size());
		u32 attributeSize = 0;
		for (u32 i = 0; auto& format: info.vertexAttributes) {
			// attributeDescs[i++] = VkVertexInputAttributeDescription{ // This gives a bug with gcc (.location starts at 1, not 0)
			attributeDescs[i] = VkVertexInputAttributeDescription{
				.location = i,
				.binding = 0,
				.format = VkFormat(format),
				.offset = attributeSize
			};
			switch (format) {
			case Format::eR32G32Sfloat:       attributeSize += 2 * sizeof(float); break;
			case Format::eR32G32B32Sfloat:    attributeSize += 3 * sizeof(float); break;
			case Format::eR32G32B32A32Sfloat: attributeSize += 4 * sizeof(float); break;
			default: VB_ASSERT(false, "Invalid Vertex Attribute"); break;
			}
			i++; // So we move it here
		}

		VkVertexInputBindingDescription bindingDescription{
			.binding   = 0,
			.stride    = attributeSize,
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		};

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &bindingDescription,
			.vertexAttributeDescriptionCount = static_cast<u32>(attributeDescs.size()),
			.pVertexAttributeDescriptions = attributeDescs.data(),
		};

		// define the type of input of our pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = info.line_topology? VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			// with this parameter true we can break up lines and triangles in _STRIP topology modes
			.primitiveRestartEnable = VK_FALSE,
		};

		VkPipelineDynamicStateCreateInfo dynamicInfo{
			.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = static_cast<u32>(info.dynamicStates.size()),
			.pDynamicStates    = reinterpret_cast<const VkDynamicState*>(info.dynamicStates.data()),
		};

		VkPipelineViewportStateCreateInfo viewportState{
			.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.pViewports    = nullptr,
			.scissorCount  = 1,
			.pScissors     = nullptr,
		};

		VkPipelineRenderingCreateInfoKHR pipelineRendering{
			.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
			.viewMask                = 0,
			.colorAttachmentCount    = static_cast<u32>(info.color_formats.size()),
			.pColorAttachmentFormats = reinterpret_cast<const VkFormat*>(info.color_formats.data()),
			.depthAttachmentFormat   = info.use_depth ? VkFormat(info.depth_format) : VK_FORMAT_UNDEFINED,
			.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
		};

		std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(info.color_formats.size(), {
			.blendEnable         = VK_FALSE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
			.colorBlendOp        = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.colorWriteMask 	 = VK_COLOR_COMPONENT_R_BIT
								 | VK_COLOR_COMPONENT_G_BIT
								 | VK_COLOR_COMPONENT_B_BIT
								 | VK_COLOR_COMPONENT_A_BIT,
		});

		VkPipelineColorBlendStateCreateInfo colorBlendState {
			.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable   = VK_FALSE,
			.logicOp         = VK_LOGIC_OP_COPY,
			.attachmentCount = static_cast<u32>(blendAttachments.size()),
			.pAttachments    = blendAttachments.data(),
			.blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
		};

		VkGraphicsPipelineCreateInfo pipelineInfo{
			.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext               = &pipelineRendering,
			.stageCount          = static_cast<u32>(shader_stages.size()),
			.pStages             = shader_stages.data(),
			.pVertexInputState   = &vertexInputInfo,
			.pInputAssemblyState = &inputAssembly,
			.pViewportState      = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState   = &multisampleState,
			.pDepthStencilState  = &depthStencilState,
			.pColorBlendState    = &colorBlendState,
			.pDynamicState       = &dynamicInfo,
			.layout              = pipeline.resource->layout,
			.basePipelineHandle  = VK_NULL_HANDLE,
			.basePipelineIndex   = -1,
		};

		VB_VK_RESULT vkRes = vkCreateGraphicsPipelines(handle, VK_NULL_HANDLE, 1, 
			&pipelineInfo, owner->allocator, &pipeline.resource->handle);
		VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, vkRes, "Failed to create graphics pipeline!");
	}

	for (auto& shaderModule: shader_modules) {
		vkDestroyShaderModule(handle, shaderModule, owner->allocator);
	}
	return pipeline;
}

auto DeviceResource::GetOrCreateSampler(SamplerInfo const& info) -> VkSampler {
	auto it = samplers.find(info);
	if (it != samplers.end()) [[likely]] {
		return it->second;
	}

	VkSamplerCreateInfo samplerInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext  = nullptr,
		.flags  = 0,
		.magFilter = VkFilter(info.magFilter),
		.minFilter = VkFilter(info.minFilter),
		.mipmapMode = VkSamplerMipmapMode(info.mipmapMode),
		.addressModeU = VkSamplerAddressMode(info.wrap.u),
		.addressModeV = VkSamplerAddressMode(info.wrap.v),
		.addressModeW = VkSamplerAddressMode(info.wrap.w),
		.mipLodBias = 0.0f,

		.anisotropyEnable = info.anisotropyEnable == false
						? VK_FALSE
						: physicalDevice->physicalFeatures2.features.samplerAnisotropy,
		.maxAnisotropy = info.maxAnisotropy,
		.compareEnable = info.compareEnable == false ? VK_FALSE : VK_TRUE,
		.compareOp = VkCompareOp(info.compareOp),

		.minLod = info.minLod,
		.maxLod = info.maxLod,

		.borderColor = VkBorderColor(info.borderColor),
		.unnormalizedCoordinates = info.unnormalizedCoordinates,
	};

	VkSampler sampler = VK_NULL_HANDLE;
	VB_VK_RESULT result = vkCreateSampler(handle, &samplerInfo, nullptr, &sampler);
	VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create texture sampler!");

	samplers.emplace(info, sampler);
	return sampler;
}

void DeviceResource::SetDebugUtilsObjectName(VkDebugUtilsObjectNameInfoEXT const& pNameInfo) {
	if (vkSetDebugUtilsObjectNameEXT) {
		vkSetDebugUtilsObjectNameEXT(handle, &pNameInfo);
	}
}

auto DeviceResource::GetName() const -> char const* {
	return name.data();
}

auto DeviceResource::GetType() const -> char const* {
	return "DeviceResource";
}

auto DeviceResource::GetInstance() const -> InstanceResource* {
	return owner;
}

void DeviceResource::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
	vkDeviceWaitIdle(handle);

	if (imguiDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(handle, imguiDescriptorPool, owner->allocator);
	}

	pipelineLibrary.Free();

	VB_LOG_TRACE("[ Free ] Cleaning up %zu device resources...", resources.size());

	std::erase_if(resources, [](ResourceBase<DeviceResource>* const res) {
		res->Free();
		return 1;
	});

	for (auto& [_, sampler] : samplers) {
		vkDestroySampler(handle, sampler, owner->allocator);
	}

	vmaDestroyAllocator(vmaAllocator);

	vkDestroyDevice(handle, owner->allocator);
	VB_LOG_TRACE("[ Free ] Destroyed logical device");
	handle = VK_NULL_HANDLE;
}
#pragma endregion
}; // namespace VB_NAMESPACE
