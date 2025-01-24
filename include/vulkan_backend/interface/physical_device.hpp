#pragma once

#ifndef VB_USE_STD_MODULE
#include <string_view>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/types.hpp"
#include "vulkan_backend/classes/no_copy_no_move.hpp"
#include "vulkan_backend/structs/command.hpp"
#include "vulkan_backend/classes/structure_chain.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
class PhysicalDevice : public vk::PhysicalDevice, NoCopy {
public:
	// using vk::PhysicalDevice::PhysicalDevice;
	PhysicalDevice(vk::PhysicalDevice const& physical_device) : vk::PhysicalDevice(physical_device) {};
	static constexpr u32 kQueueNotFound = ~0u;
	// Get all information from Vulkan API
	void GetDetails();

	// Getters
	inline auto GetFeatures()          -> vk::PhysicalDeviceFeatures&          { return base_features.features2.features; }
	inline auto GetFeatures2()         -> vk::PhysicalDeviceFeatures2&         { return base_features.features2; }
	inline auto GetProperties()        -> vk::PhysicalDeviceProperties&        { return base_properties.properties2.properties; }
	inline auto GetProperties2()       -> vk::PhysicalDeviceProperties2&       { return base_properties.properties2; }
	inline auto GetFeatures()    const -> vk::PhysicalDeviceFeatures    const& { return base_features.features2.features; }
	inline auto GetFeatures2()   const -> vk::PhysicalDeviceFeatures2   const& { return base_features.features2; }
	inline auto GetProperties()  const -> vk::PhysicalDeviceProperties  const& { return base_properties.properties2.properties; }
	inline auto GetProperties2() const -> vk::PhysicalDeviceProperties2 const& { return base_properties.properties2; }
	inline auto GetMemoryProperties()  -> vk::PhysicalDeviceMemoryProperties2 const& { return memory_properties; }
	
	// Get best fitting queue family index or error code if strict requirements not met
	auto GetQueueFamilyIndex(QueueFamilyIndexRequest const& request) -> std::pair<QueueFamilyIndexRequest::Result, u32>;
	bool SupportsExtension(std::string_view extension) const;
	bool SupportsExtensions(std::span<char const* const> extensions) const;
	bool SupportsQueues(std::span<QueueInfo const> queues) const;
	bool SupportsSurface(u32 queue_family_index, vk::SurfaceKHR const surface) const;
	auto GetDedicatedTransferQueueFamilyIndex() const -> u32;
	auto GetDedicatedComputeQueueFamilyIndex() const -> u32;
	auto GetQueueCount(u32 queue_family_index) const -> u32;

	auto FilterSupportedExtensions(std::span<char const* const> extensions) const -> std::vector<std::string_view>;
	bool SupportsRequiredFeatures() const;
	// Only fills:
	// .queueFamilyIndex
	// .queueCount
	auto GetQueueCreateInfos(std::span<QueueInfo const> infos)
		-> std::vector<vk::DeviceQueueCreateInfo>;

	auto GetName() const -> char const*;
	auto GetResourceTypeName() const -> char const*;

	// In derived classes add structure to their pNext chain with AppendStructureChain()
	// Example: In PhysicalDeviceDerived add member:
	// vk::PhysicalDeviceShaderAtomicFloatFeaturesEXT atomic_features;
	// In PhysicalDeviceDerived constructor call:
	// base_features.LinkNextStructure(reinterpret_cast<vk::BaseOutStructure*>(&atomic_features));
	
	// Features
	FeatureChain base_features{true};

	// Properties
	PropertiesChain base_properties;

	// Memory properties
	vk::PhysicalDeviceMemoryProperties2 memory_properties;

	// Queue properties
	std::vector<vk::QueueFamilyProperties2> queue_family_properties;
	
	// Extensions
	std::vector<vk::ExtensionProperties> available_extensions;

	// Max supported samples, updated with GetDetails();
	vk::SampleCountFlagBits max_samples = vk::SampleCountFlagBits::e1;
};
} // namespace VB_NAMESPACE

