#ifndef VULKAN_BACKEND_RESOURCE_PHYSICAL_DEVICE_HPP_
#define VULKAN_BACKEND_RESOURCE_PHYSICAL_DEVICE_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <set>
#include <string_view>
#else
import std;
#endif

#include <vulkan/vulkan.h>

#include "vulkan_backend/fwd.hpp"
#include "common.hpp"

namespace VB_NAMESPACE {
struct PhysicalDeviceResource : NoCopy {
	InstanceResource* instance = nullptr;
	VkPhysicalDevice handle = VK_NULL_HANDLE;

	// Features
	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT unusedAttachmentFeatures;
	VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphicsPipelineLibraryFeatures;
	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures;
	VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features;
	VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures;
	VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures;
	VkPhysicalDeviceFeatures2 physicalFeatures2;

	// Properties
	VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT graphicsPipelineLibraryProperties;
	VkPhysicalDeviceProperties2 physicalProperties2;
	VkPhysicalDeviceMemoryProperties memoryProperties;

	// Extensions
	std::vector<VkExtensionProperties> availableExtensions;

	// Families
	std::vector<VkQueueFamilyProperties> availableFamilies;

	// Max samples
	SampleCount maxSamples = SampleCount::e1;

	// desiredFlags: required queue flags (strict)
	// undesiredFlags: undesired queue flags (strict)
	// avoidIfPossible: vector of pairs (flags, priority to avoid)
	// surfaceToSupport: surface to support (strict)
	// numTakenQueues: numbers of already taken queues in families (optional)
	// this is rather complicated, but necessary
	struct QueueFamilyIndexRequest {
		struct AvoidInfo{
			VkQueueFlags flags;
			float penalty;
		};
		VkQueueFlags desiredFlags;
		VkQueueFlags undesiredFlags;
		std::span<AvoidInfo const> avoidIfPossible = {};
		VkSurfaceKHR surfaceToSupport = VK_NULL_HANDLE;
		std::span<u32 const> numTakenQueues = {};
	};
	
	u32 static constexpr QUEUE_NOT_FOUND = ~0u;
	u32 static constexpr ALL_SUITABLE_QUEUES_TAKEN = ~1u;

	// @return best fitting queue family index or error code if strict requirements not met
	auto GetQueueFamilyIndex(QueueFamilyIndexRequest const& request) -> u32;
	void GetDetails();
	auto FilterSupported(std::span<char const*> extensions) -> std::vector<std::string_view>;
	auto SupportsExtensions(std::span<char const*> extensions) -> bool;
	PhysicalDeviceResource(InstanceResource* instance, VkPhysicalDevice handle = VK_NULL_HANDLE)
		: instance(instance), handle(handle) {};

	auto GetName() const -> char const*;
	auto GetType() const -> char const*;
	auto GetInstance() -> InstanceResource*;
};

} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_RESOURCE_PHYSICAL_DEVICE_HPP_