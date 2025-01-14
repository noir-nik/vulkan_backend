#ifndef VB_USE_STD_MODULE
#include <string_view>
#include <algorithm>
#include <vector>
#include <span>
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "physical_device.hpp"
#include "instance.hpp"
#include "../macros.hpp"


namespace VB_NAMESPACE {
void PhysicalDeviceResource::GetDetails() {
	// get all available extensions
	u32 extensionCount;
	vkEnumerateDeviceExtensionProperties(handle, nullptr, &extensionCount, nullptr);
	availableExtensions.resize(extensionCount);
	vkEnumerateDeviceExtensionProperties(handle, nullptr, &extensionCount, availableExtensions.data());

	// get all available families
	u32 familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(handle, &familyCount, nullptr);
	availableFamilies.resize(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(handle, &familyCount, availableFamilies.data());

	// get features
	unusedAttachmentFeatures        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT, nullptr };
	graphicsPipelineLibraryFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT, &unusedAttachmentFeatures };
	indexingFeatures                = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,       &graphicsPipelineLibraryFeatures };
	bufferDeviceAddressFeatures     = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,         &indexingFeatures };
	synchronization2Features        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,         &bufferDeviceAddressFeatures };
	dynamicRenderingFeatures        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,             &synchronization2Features };
	physicalFeatures2               = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,                             &dynamicRenderingFeatures };
	vkGetPhysicalDeviceFeatures2(handle, &physicalFeatures2);

	// get properties
	graphicsPipelineLibraryProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT;
	graphicsPipelineLibraryProperties.pNext = nullptr;
	physicalProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	physicalProperties2.pNext = &graphicsPipelineLibraryProperties;
	vkGetPhysicalDeviceProperties2(handle, &physicalProperties2);
	vkGetPhysicalDeviceMemoryProperties(handle, &memoryProperties);

	VkSampleCountFlags counts = physicalProperties2.properties.limits.framebufferColorSampleCounts;

	// Get max number of samples
	for (VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_64_BIT;
			sampleCount >= VK_SAMPLE_COUNT_1_BIT;
				sampleCount = static_cast<VkSampleCountFlagBits>(sampleCount >> 1)) {
		if (counts & sampleCount) {
			maxSamples = static_cast<SampleCount>(sampleCount);
			break;
		}
	}
}

auto PhysicalDeviceResource::FilterSupported(std::span<char const*> extensions) -> std::vector<std::string_view> {
	std::vector<std::string_view> extensionsToEnable;
	extensionsToEnable.reserve(extensions.size());
	std::copy_if(extensions.begin(), extensions.end(), std::back_inserter(extensionsToEnable),
		[&](char const* extension) {
			if (std::any_of(availableExtensions.begin(), availableExtensions.end(),
				[&extension](auto const& availableExtension) {
					return std::string_view(availableExtension.extensionName) == extension;
			}) == true) {
				return true; 
			} else {
				VB_LOG_WARN("Extension %s not supported on device %s",
					extension, physicalProperties2.properties.deviceName);
				return false;
			}
		});
	return extensionsToEnable;
}

auto PhysicalDeviceResource::SupportsExtensions(std::span<char const*> extensions) -> bool {
	return std::all_of(extensions.begin(), extensions.end(), [this](auto const& extension) {
		return std::any_of(availableExtensions.begin(), availableExtensions.end(),
			[&extension](auto const& availableExtension) {
				return std::string_view(availableExtension.extensionName) == extension;
			});
	});
}

auto PhysicalDeviceResource::GetQueueFamilyIndex(const QueueFamilyIndexRequest& params) -> u32 {
	struct Candidate {
		u32   familyIndex;
		float penalty;
	};
	
	std::vector<Candidate> candidates;
	auto& families = availableFamilies;
	
	// Check if queue family is suitable
	for (size_t i = 0; i < families.size(); i++) {
		bool suitable = ((families[i].queueFlags & params.desiredFlags) == params.desiredFlags &&
						((families[i].queueFlags & params.undesiredFlags) == 0));

		// Get surface support
		if (params.surfaceToSupport != VK_NULL_HANDLE) {
			VkBool32 presentSupport = false;
			VB_VK_RESULT result = vkGetPhysicalDeviceSurfaceSupportKHR(handle, i, params.surfaceToSupport, &presentSupport);
			VB_CHECK_VK_RESULT(instance->init_info.checkVkResult, result, "vkGetPhysicalDeviceSurfaceSupportKHR failed!");
			suitable = suitable && presentSupport;
		}

		if (!suitable) {
			continue;
		}

		if (params.avoidIfPossible.empty()) {
			return i;
		}

		Candidate candidate(i, 0.0f);
		
		// Prefer family with least number of VkQueueFlags
		for (VkQueueFlags bit = 1; bit != 0; bit <<= 1) {
			if (families[i].queueFlags & bit) {
				candidate.penalty += 0.01f;
			}
		}

		// Add penalty for supporting unwanted VkQueueFlags
		for (auto& avoid: params.avoidIfPossible) {
			if ((families[i].queueFlags & avoid.flags) == avoid.flags) {
				candidate.penalty += avoid.penalty;
			}
		}
		
		// Add candidate if family is not filled up
		if (params.numTakenQueues[i] < families[i].queueCount) {
			candidates.push_back(candidate);
		} else {
			VB_LOG_WARN("Queue family %zu is filled up", i);
		}
	}

	if (candidates.empty()) {
		return ALL_SUITABLE_QUEUES_TAKEN;
	}

	u32 bestFamily = std::min_element(candidates.begin(), candidates.end(),
							[](Candidate& l, Candidate& r) {
									return l.penalty < r.penalty; 
								})->familyIndex;
	VB_LOG_TRACE("Best family: %u", bestFamily);
	return bestFamily;
}

auto PhysicalDeviceResource::GetName() const -> char const* {
	return physicalProperties2.properties.deviceName;
}

auto PhysicalDeviceResource::GetType() const -> char const* {
	return "PhysicalDeviceResource";
}

auto PhysicalDeviceResource::GetInstance() -> InstanceResource* {
	return instance;
}
} // namespace VB_NAMESPACE