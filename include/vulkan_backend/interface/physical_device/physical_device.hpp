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
#include "vulkan_backend/classes/structure_chain.hpp"
#include "vulkan_backend/interface/physical_device/info.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
class PhysicalDevice : public vk::PhysicalDevice, NoCopy {
public:
	// using vk::PhysicalDevice::PhysicalDevice;
	PhysicalDevice(vk::PhysicalDevice const& physical_device) : vk::PhysicalDevice(physical_device) {};
	static constexpr u32 kQueueNotFound = ~0u;
	// Get all data from Vulkan API (features, properties, available
	// extensions, memory properties, queue families)
	void GetDetails();

	// Getters
	inline auto GetFeatureChain()      -> FeatureChainBase&                    { return base_features; }
	inline auto GetPropertiesChain()   -> PropertiesChainBase&                 { return base_properties; }
	inline auto GetFeatures()          -> vk::PhysicalDeviceFeatures&          { return base_features.features2.features; }
	inline auto GetFeatures2()         -> vk::PhysicalDeviceFeatures2&         { return base_features.features2; }
	inline auto GetProperties()        -> vk::PhysicalDeviceProperties&        { return base_properties.properties2.properties; }
	inline auto GetProperties2()       -> vk::PhysicalDeviceProperties2&       { return base_properties.properties2; }
	inline auto GetFeatures()    const -> vk::PhysicalDeviceFeatures    const& { return base_features.features2.features; }
	inline auto GetFeatures2()   const -> vk::PhysicalDeviceFeatures2   const& { return base_features.features2; }
	inline auto GetProperties()  const -> vk::PhysicalDeviceProperties  const& { return base_properties.properties2.properties; }
	inline auto GetProperties2() const -> vk::PhysicalDeviceProperties2 const& { return base_properties.properties2; }
	inline auto GetMemoryProperties()  -> vk::PhysicalDeviceMemoryProperties2 const& { return memory_properties; }
	inline auto GetQueueProperties()   -> std::span<vk::QueueFamilyProperties2> { return queue_family_properties; }
	
	bool SupportsExtension(std::string_view extension) const;
	bool SupportsExtensions(std::span<char const* const> extensions) const;
	bool SupportsQueue(QueueInfo const& queue_info) const;
	bool SupportsQueues(std::span<QueueInfo const> queue_infos) const;
	bool SupportsSurface(u32 queue_family_index, vk::SurfaceKHR const surface) const;
	bool Supports(PhysicalDeviceSupportInfo const& info = {}) const;

	// Some feature support checks
	bool SupportsDynamicRendering() const;
	bool SupportsSynchronization2() const;
	bool SupportsBufferDeviceAddress() const;
	
	// returns nullptr if not found
	auto GetDedicatedTransferQueueFamily() const -> vk::QueueFamilyProperties const*;
	auto GetDedicatedComputeQueueFamily() const -> vk::QueueFamilyProperties const*;
	auto GetQueueCount(u32 queue_family_index) const -> u32;
	auto GetMaxPushConstantsSize() const -> u32;

	// Returns fitting queue family index or kQueueNotFound
	auto FindQueueFamilyIndex(QueueInfo const& info, vk::QueueFlags undesired_flags = {})
		-> u32;
	// auto GetQueueFamilyIndex(vk::QueueFamilyProperties* queue_family) const -> u32;
	auto FilterSupportedExtensions(std::span<char const* const> extensions) const
		-> std::vector<std::string_view>;
	bool SupportsRequiredFeatures() const;
	// Only fills:
	// .queueFamilyIndex
	// .queueCount
	// auto GetQueueCreateInfos(std::span<QueueInfo const> infos)
		// -> std::vector<vk::DeviceQueueCreateInfo>;

	auto GetName() const -> char const*;
	auto GetResourceTypeName() const -> char const*;

	// In derived classes add structure to their pNext chain with AppendStructureChain()
	// Example: In PhysicalDeviceDerived add member:
	// vk::PhysicalDeviceShaderAtomicFloatFeaturesEXT atomic_features;
	// In PhysicalDeviceDerived constructor call:
	// base_features.LinkNextStructure(reinterpret_cast<vk::BaseOutStructure*>(&atomic_features));

	// Features
	FeatureChainBase base_features;

	// Properties
	PropertiesChainBase base_properties;

	// Memory properties
	vk::PhysicalDeviceMemoryProperties2 memory_properties;

	// Queue properties
	std::vector<vk::QueueFamilyProperties2> queue_family_properties;
	
	// Extensions
	std::vector<vk::ExtensionProperties> available_extensions;

	// Max supported samples, updated with GetDetails();
	vk::SampleCountFlagBits max_samples = vk::SampleCountFlagBits::e1;
private:
  bool IsQueueFamilySuitable(u32 queue_family_index, QueueInfo const& queue) const;
};
} // namespace VB_NAMESPACE

