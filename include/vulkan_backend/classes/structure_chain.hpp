#pragma once

#ifndef VB_USE_STD_MODULE
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"

template <typename T>
concept VulkanStructureT = requires(T t) { t.pNext; };

// Safe addition to a pNext chain
template <VulkanStructureT ExistingStructure, VulkanStructureT NewStructure>
inline void AddToPNext(ExistingStructure& existing, NewStructure& new_structure) {
	new_structure.pNext = existing.pNext;
	existing.pNext = &new_structure;
}

VB_EXPORT
namespace VB_NAMESPACE {
constexpr void EnableRequiredFeatures(vk::PhysicalDeviceVulkan12Features& features) {
	features.shaderUniformBufferArrayNonUniformIndexing	   = true;
	features.shaderSampledImageArrayNonUniformIndexing	   = true;
	features.shaderStorageBufferArrayNonUniformIndexing	   = true;
	features.descriptorBindingSampledImageUpdateAfterBind  = true;
	features.descriptorBindingStorageImageUpdateAfterBind  = true;
	features.descriptorBindingStorageBufferUpdateAfterBind = true;
	features.descriptorBindingPartiallyBound			   = true;
	features.runtimeDescriptorArray						   = true;
	features.bufferDeviceAddress						   = true;
}

constexpr void EnableRequiredFeatures(vk::PhysicalDeviceVulkan13Features& features) {
	features.synchronization2 = true;
	features.dynamicRendering = true;
}

struct FeatureChainBase {
	constexpr inline FeatureChainBase() {
		features2.pNext = &vulkan11;
		vulkan11.pNext	= &vulkan12;
		vulkan12.pNext	= &vulkan13;
	};

	// Pointer is assumed to be valid
	template <VulkanStructureT T>
	constexpr inline void LinkNextStructure(T& next) {
		AddToPNext(GetBase(), next);
	};

	constexpr inline auto GetBase() -> vk::PhysicalDeviceFeatures2& { return features2; }
	constexpr inline auto GetCore10() -> vk::PhysicalDeviceFeatures& { return features2.features; }
	constexpr inline auto GetCore11() -> vk::PhysicalDeviceVulkan11Features& { return vulkan11; }
	constexpr inline auto GetCore12() -> vk::PhysicalDeviceVulkan12Features& { return vulkan12; }
	constexpr inline auto GetCore13() -> vk::PhysicalDeviceVulkan13Features& { return vulkan13; }

	constexpr inline auto GetBase() const -> vk::PhysicalDeviceFeatures2 const& { return features2; }
	constexpr inline auto GetCore10() const -> vk::PhysicalDeviceFeatures const& { return features2.features; }
	constexpr inline auto GetCore11() const -> vk::PhysicalDeviceVulkan11Features const& { return vulkan11; }
	constexpr inline auto GetCore12() const -> vk::PhysicalDeviceVulkan12Features const& { return vulkan12; }
	constexpr inline auto GetCore13() const -> vk::PhysicalDeviceVulkan13Features const& { return vulkan13; }

  private:
	vk::PhysicalDeviceFeatures2		   features2;
	vk::PhysicalDeviceVulkan11Features vulkan11;
	vk::PhysicalDeviceVulkan12Features vulkan12;
	vk::PhysicalDeviceVulkan13Features vulkan13;
};

struct PropertiesChainBase {
	// Setup structure chain in constructor
	constexpr inline PropertiesChainBase() {
		properties2.pNext = &vulkan11;
		vulkan11.pNext	  = &vulkan12;
		vulkan12.pNext	  = &vulkan13;
	}

	template <VulkanStructureT T>
	inline void LinkNextStructure(T& next) {
		AddToPNext(GetBase(), next);
	};

	constexpr inline auto GetBase() -> vk::PhysicalDeviceProperties2& { return properties2; }
	constexpr inline auto GetCore10() -> vk::PhysicalDeviceProperties& { return properties2.properties; }
	constexpr inline auto GetCore11() -> vk::PhysicalDeviceVulkan11Properties& { return vulkan11; }
	constexpr inline auto GetCore12() -> vk::PhysicalDeviceVulkan12Properties& { return vulkan12; }
	constexpr inline auto GetCore13() -> vk::PhysicalDeviceVulkan13Properties& { return vulkan13; }

	constexpr inline auto GetBase() const -> vk::PhysicalDeviceProperties2 const& { return properties2; }
	constexpr inline auto GetCore10() const -> vk::PhysicalDeviceProperties const& { return properties2.properties; }
	constexpr inline auto GetCore11() const -> vk::PhysicalDeviceVulkan11Properties const& { return vulkan11; }
	constexpr inline auto GetCore12() const -> vk::PhysicalDeviceVulkan12Properties const& { return vulkan12; }
	constexpr inline auto GetCore13() const -> vk::PhysicalDeviceVulkan13Properties const& { return vulkan13; }

  private:
	vk::PhysicalDeviceProperties2		 properties2; // Vulkan 1.0 properties
	vk::PhysicalDeviceVulkan11Properties vulkan11;	  // Vulkan 1.1 properties
	vk::PhysicalDeviceVulkan12Properties vulkan12;	  // Vulkan 1.2 properties
	vk::PhysicalDeviceVulkan13Properties vulkan13;	  // Vulkan 1.3 properties
};
} // namespace VB_NAMESPACE
#pragma once
