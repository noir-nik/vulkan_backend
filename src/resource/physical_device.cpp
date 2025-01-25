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

#include "vulkan_backend/interface/physical_device/physical_device.hpp"
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
		for (auto [i, family_property] : util::enumerate(queue_family_properties)) {
			bool has_available_queues = num_taken_queues[i] < family_property.queueFamilyProperties.queueCount;
			bool has_desired_flags = TestBits(family_property.queueFamilyProperties.queueFlags, queue.flags);
			bool has_undesired_flags = (family_property.queueFamilyProperties.queueFlags & queue.undesired_flags) != vk::QueueFlags{};
			bool supports_surface = queue.supported_surface == vk::SurfaceKHR{} || SupportsSurface(i, queue.supported_surface);

			if (has_available_queues && has_desired_flags && !has_undesired_flags && supports_surface) {
				++num_taken_queues[i];
				VB_LOG_TRACE("Using queue family %u", i);
				return true;
			}

		}
		VB_LOG_WARN("Failed to find suitable queue family");
		return false;
	});
}

auto PhysicalDevice::IsSuitable(PhysicalDeviceSelectInfo const& info) const -> bool {
	return SupportsExtensions(info.extensions) && SupportsRequiredFeatures() && SupportsQueues(info.queues);
}

auto PhysicalDevice::GetDedicatedTransferQueueFamily() const -> vk::QueueFamilyProperties const* {
	for (auto [index, family] : util::enumerate(queue_family_properties)) {
		if (TestBits(family.queueFamilyProperties.queueFlags, vk::QueueFlagBits::eTransfer) &&
			!TestBits(family.queueFamilyProperties.queueFlags, vk::QueueFlagBits::eGraphics) &&
			!TestBits(family.queueFamilyProperties.queueFlags, vk::QueueFlagBits::eCompute)) {
			return &family.queueFamilyProperties;
		}
	}
	return nullptr;
}

auto PhysicalDevice::GetDedicatedComputeQueueFamily() const -> vk::QueueFamilyProperties const* {
	for (auto [index, family] : util::enumerate(queue_family_properties)) {
		if (TestBits(family.queueFamilyProperties.queueFlags, vk::QueueFlagBits::eCompute) &&
			!TestBits(family.queueFamilyProperties.queueFlags, vk::QueueFlagBits::eGraphics)) {
			return &family.queueFamilyProperties;
		}
	}
	return nullptr;
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
/* 
auto PhysicalDevice::GetQueueCreateInfos(std::span<QueueInfo const> infos)
	-> std::vector<vk::DeviceQueueCreateInfo> {
	VB_VLA(u32, num_taken_queues, queue_family_properties.size());
	std::fill(num_taken_queues.begin(), num_taken_queues.end(), 0);

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
 */
auto PhysicalDevice::FindQueueFamilyIndex(QueueInfo const& info, vk::QueueFlags undesired_flags)
	-> u32 {

	// Check if queue family is suitable
	for (auto [i, family] : util::enumerate(queue_family_properties)) {
		bool desiredFlagsSet = TestBits(family.queueFamilyProperties.queueFlags, info.flags);
		bool undesiredFlagsSet = (family.queueFamilyProperties.queueFlags & undesired_flags) != vk::QueueFlags{};
		bool supportsSurface = info.supported_surface == vk::SurfaceKHR{} || SupportsSurface(i, info.supported_surface);
		if (desiredFlagsSet && !undesiredFlagsSet && supportsSurface) {
			VB_LOG_TRACE("Best family: %u", i);
			return i;
		}
	}
	return kQueueNotFound;
}

auto PhysicalDevice::GetName() const -> char const* {
	return GetProperties2().properties.deviceName;
}

auto PhysicalDevice::GetResourceTypeName() const -> char const* {
	return "PhysicalDeviceResource";
}

} // namespace VB_NAMESPACE