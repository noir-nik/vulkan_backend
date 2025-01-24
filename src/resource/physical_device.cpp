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

#include "vulkan_backend/interface/physical_device.hpp"
#include "vulkan_backend/functions.hpp"
#include "vulkan_backend/vk_result.hpp"
#include "vulkan_backend/util/algorithm.hpp"
#include "vulkan_backend/util/enumerate.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/util/bits.hpp"
#include "vulkan_backend/log.hpp"



namespace VB_NAMESPACE {
void PhysicalDevice::GetDetails() {
	// get all available extensions
	auto [result, extensions] = enumerateDeviceExtensionProperties();
	available_extensions = std::move(extensions);
	VB_CHECK_VK_RESULT(result, "Failed to enumerate device extensions!");

	// get features, properties and queue families
	getFeatures2(&GetFeatures2());
	getProperties2(&GetProperties2());
	getMemoryProperties2(&memory_properties);
	queue_family_properties = getQueueFamilyProperties2();

	// Get max number of samples
	vk::SampleCountFlags sample_counts = GetProperties2().properties.limits.framebufferColorSampleCounts;
	for (vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e64; sampleCount >= vk::SampleCountFlagBits::e1;
		 sampleCount = static_cast<vk::SampleCountFlagBits>(vk::SampleCountFlags::MaskType(sampleCount) >> 1)) {
		if (sample_counts & sampleCount) {
			max_samples = static_cast<vk::SampleCountFlagBits>(sampleCount);
			break;
		}
	}
}

auto PhysicalDevice::FilterSupportedExtensions(std::span<char const* const> extensions) const -> std::vector<std::string_view> {
	std::vector<std::string_view> extensionsToEnable;
	extensionsToEnable.reserve(extensions.size());
	std::copy_if(extensions.begin(), extensions.end(), std::back_inserter(extensionsToEnable),
		[&](char const* extension) {
			if (std::any_of(available_extensions.begin(), available_extensions.end(),
				[&extension](auto const& availableExtension) {
					return std::string_view(availableExtension.extensionName) == extension;
			}) == true) {
				return true; 
			} else {
				VB_LOG_WARN("Extension %s not supported on device %s",
					extension, GetProperties2().properties.deviceName);
				return false;
			}
		});
	return extensionsToEnable;
}

auto PhysicalDevice::SupportsExtension(std::string_view extension) const -> bool {
	return algo::SupportsExtension(available_extensions, extension);
}

auto PhysicalDevice::SupportsExtensions(std::span<char const* const> extensions) const -> bool {
	return algo::SupportsExtensions(available_extensions, extensions);
}

auto PhysicalDevice::SupportsSurface(u32 queue_family_index, vk::SurfaceKHR const surface) const -> bool {
		auto [result, supported] = getSurfaceSupportKHR(queue_family_index, surface);
		VB_CHECK_VK_RESULT(result, "Failed to get surface support");
		return supported;
}

auto PhysicalDevice::SupportsQueues(std::span<QueueInfo const> queues) const -> bool {
	VB_VLA(u32, num_taken_queues, queue_family_properties.size());
	std::fill(num_taken_queues.begin(), num_taken_queues.end(), 0);
	return std::all_of(queues.begin(), queues.end(), [this, &num_taken_queues](auto const& queue) {
		for (u32 i = 0; i < queue_family_properties.size(); ++i) {
			auto& family_properties = queue_family_properties[i].queueFamilyProperties;
			bool has_available_queues = num_taken_queues[i] < family_properties.queueCount;
			bool flags_supported = TestBits(family_properties.queueFlags, queue.flags);
			bool no_supported_surface_required = queue.supported_surface == vk::SurfaceKHR{};
			bool surface_supported = no_supported_surface_required || SupportsSurface(i, queue.supported_surface);

			if (has_available_queues && flags_supported && surface_supported) {
				++num_taken_queues[i];
				VB_LOG_TRACE("Using queue family %u", i);
				return true;
			}

		}
		VB_LOG_WARN("Failed to find suitable queue family");
		return false;
	});
}

auto PhysicalDevice::GetDedicatedTransferQueueFamilyIndex() const -> u32 {
	for (auto [index, family] : util::enumerate(queue_family_properties)) {
		if (TestBits(family.queueFamilyProperties.queueFlags, vk::QueueFlagBits::eTransfer) &&
			!TestBits(family.queueFamilyProperties.queueFlags, vk::QueueFlagBits::eGraphics)) {
			return index;
		}
	}
	return kQueueNotFound;
}

auto PhysicalDevice::GetDedicatedComputeQueueFamilyIndex() const -> u32 {
	for (auto [index, family] : util::enumerate(queue_family_properties)) {
		if (TestBits(family.queueFamilyProperties.queueFlags, vk::QueueFlagBits::eCompute) &&
			!TestBits(family.queueFamilyProperties.queueFlags, vk::QueueFlagBits::eGraphics)) {
			return index;
		}
	}
	return kQueueNotFound;
}

auto PhysicalDevice::GetQueueCount(u32 queue_family_index) const -> u32 {
	return queue_family_properties[queue_family_index].queueFamilyProperties.queueCount;
}

auto PhysicalDevice::SupportsRequiredFeatures() const -> bool {
	if (// descriptor indexing
		base_features.vulkan12.shaderUniformBufferArrayNonUniformIndexing &&
		base_features.vulkan12.shaderSampledImageArrayNonUniformIndexing &&
		base_features.vulkan12.shaderStorageBufferArrayNonUniformIndexing &&
		base_features.vulkan12.descriptorBindingSampledImageUpdateAfterBind &&
		base_features.vulkan12.descriptorBindingStorageImageUpdateAfterBind &&
		base_features.vulkan12.descriptorBindingPartiallyBound &&
		base_features.vulkan12.runtimeDescriptorArray &&
		// buffer device address
		base_features.vulkan12.bufferDeviceAddress &&
		// dynamic rendering
		base_features.vulkan13.dynamicRendering &&
		// synchronization2
		base_features.vulkan13.synchronization2) {
		return true;
	}
	return false;
}

auto PhysicalDevice::GetQueueCreateInfos(std::span<QueueInfo const> infos)
	-> std::vector<vk::DeviceQueueCreateInfo> {
	VB_VLA(u32, num_taken_queues, queue_family_properties.size());
	std::fill(num_taken_queues.begin(), num_taken_queues.end(), 0);

	for (auto& queue_info : infos) {
		// Queue choosing heuristics
		using AvoidInfo = QueueFamilyIndexRequest::AvoidInfo;
		std::span<AvoidInfo const> avoid_if_possible;
		if (queue_info.separate_family) {
			switch (vk::QueueFlags::MaskType(queue_info.flags)) {
			case VK_QUEUE_COMPUTE_BIT: 
				avoid_if_possible = QueueFamilyIndexRequest::kAvoidCompute;
				break;
			case VK_QUEUE_TRANSFER_BIT:
				avoid_if_possible = QueueFamilyIndexRequest::kAvoidTransfer;
				break;
			default:
				avoid_if_possible = QueueFamilyIndexRequest::kAvoidOther;
				break;
			}
		}
		// Get family index fitting requirements
		QueueFamilyIndexRequest request{
			.desired_flags    = queue_info.flags,
			.undesired_flags  = {},
			.avoid_if_possible = avoid_if_possible,			
			.num_taken_queues  = num_taken_queues
		};
		if (queue_info.supported_surface) {
			request.surface_to_support = queue_info.supported_surface;
		}

		auto const [result, family_index] = GetQueueFamilyIndex(request);
		if (result == QueueFamilyIndexRequest::Result::kNoSuitableQueue) {
			VB_LOG_WARN("Requested queue flags %d not available on device: %s",
				vk::QueueFlags::MaskType(queue_info.flags),
				GetProperties2().properties.deviceName);
			break;
		} else if (result == QueueFamilyIndexRequest::Result::kAllSuitableQueuesTaken) {
			VB_LOG_WARN("Requested more queues with flags %d than available on device: %s. Queue was not created",
				vk::QueueFlags::MaskType(queue_info.flags),
				GetProperties2().properties.deviceName);
			continue;
		}
		// Create queue
		++num_taken_queues[family_index];
	}

	auto num_families = std::count_if(num_taken_queues.begin(), num_taken_queues.end(),
								[](int queueCount) { return queueCount > 0; });
	// VB_VLA(vk::DeviceQueueCreateInfo, queueCreateInfos, num_families);
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos(num_families);
	for (u32 queueInfoIndex{}, familyIndex{}; familyIndex < num_taken_queues.size(); ++familyIndex) {
		if (num_taken_queues[familyIndex] == 0) continue;
		queueCreateInfos[queueInfoIndex] = vk::DeviceQueueCreateInfo{
			.queueFamilyIndex = familyIndex,
			.queueCount       = num_taken_queues[familyIndex],
		};
		++queueInfoIndex;
	}

	return queueCreateInfos;
}

auto PhysicalDevice::GetQueueFamilyIndex(const QueueFamilyIndexRequest& params)
	-> std::pair<QueueFamilyIndexRequest::Result, u32> {
	using Result = QueueFamilyIndexRequest::Result;
	struct Candidate {
		u32	  familyIndex;
		float penalty;
	};

	std::vector<Candidate> candidates;
	
	// Check if queue family is suitable
	for (auto [i, family] : util::enumerate(queue_family_properties)) {
		auto desiredFlagsSet = TestBits(family.queueFamilyProperties.queueFlags, params.desired_flags);
		auto undesiredFlagsSet = TestBits(family.queueFamilyProperties.queueFlags, params.undesired_flags);

		bool suitable = desiredFlagsSet && (!params.undesired_flags || !undesiredFlagsSet);

		// Get surface support
		if (params.surface_to_support != nullptr) {
			vk::Bool32 presentSupport = false;
			VB_VK_RESULT result = getSurfaceSupportKHR(i, params.surface_to_support, &presentSupport);
			VB_CHECK_VK_RESULT(result, "vkGetPhysicalDeviceSurfaceSupportKHR failed!");
			suitable = suitable && presentSupport;
		}

		if (!suitable) {
			continue;
		}

		if (params.avoid_if_possible.empty()) {
			return {Result::kSuccess, i};
		}

		Candidate candidate(i, 0.0f);

		// Prefer family with least number of VkQueueFlags
		for (vk::QueueFlags::MaskType bit = 1; bit != 0; bit <<= 1) {
			if (TestBits(vk::QueueFlags::MaskType(family.queueFamilyProperties.queueFlags), bit)) {
				candidate.penalty += 0.01f;
			}
		}

		// Add penalty for supporting unwanted VkQueueFlags
		for (auto& avoid: params.avoid_if_possible) {
			if (TestBits(family.queueFamilyProperties.queueFlags, avoid.flags)) {
				candidate.penalty += avoid.penalty;
			}
		}
		
		// Add candidate if family is not filled up
		if (params.num_taken_queues[i] < family.queueFamilyProperties.queueCount) {
			candidates.push_back(candidate);
		} else {
			VB_LOG_WARN("Queue family %zu is filled up", i);
		}
	}

	if (candidates.empty()) {
		return {Result::kAllSuitableQueuesTaken, ~0u};
	}

	u32 bestFamily = std::min_element(candidates.begin(), candidates.end(),
							[](Candidate& l, Candidate& r) {
									return l.penalty < r.penalty; 
								})->familyIndex;
	VB_LOG_TRACE("Best family: %u", bestFamily);
	return {Result::kSuccess, bestFamily};
}

auto PhysicalDevice::GetName() const -> char const* {
	return GetProperties2().properties.deviceName;
}

auto PhysicalDevice::GetResourceTypeName() const -> char const* {
	return "PhysicalDeviceResource";
}

} // namespace VB_NAMESPACE