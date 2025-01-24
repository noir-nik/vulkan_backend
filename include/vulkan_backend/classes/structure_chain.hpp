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

struct FeatureChain {
	constexpr FeatureChain(bool enable_required = true) {
		features2.pNext = &vulkan11;
		vulkan11.pNext = &vulkan12;
		vulkan12.pNext = &vulkan13;
		vulkan13.pNext = &graphics_pipeline_library;
		if (enable_required) {
			EnableRequiredFeatures(vulkan12);
			EnableRequiredFeatures(vulkan13);
		}
	};
	void LinkNextStructure(vk::BaseOutStructure* const next) {
		graphics_pipeline_library.pNext = next;
	};
	vk::PhysicalDeviceFeatures2                             features2;
	vk::PhysicalDeviceVulkan11Features                      vulkan11;
	vk::PhysicalDeviceVulkan12Features                      vulkan12;
	vk::PhysicalDeviceVulkan13Features                      vulkan13;
	vk::PhysicalDeviceGraphicsPipelineLibraryFeaturesEXT    graphics_pipeline_library;
};

FeatureChain inline kRequiredFeatures{true};

struct PropertiesChain {
	// Setup structure chain in constructor
	constexpr PropertiesChain() {
		properties2.pNext = &vulkan11;
		vulkan11.pNext = &vulkan12;
		vulkan12.pNext = &vulkan13;
		vulkan13.pNext = &graphics_pipeline_library;
	}
	void LinkNextStructure(vk::BaseOutStructure* const next) {
		graphics_pipeline_library.pNext = next;
	}

	vk::PhysicalDeviceProperties2                             properties2;             // Vulkan 1.0 properties
	vk::PhysicalDeviceVulkan11Properties                      vulkan11;                // Vulkan 1.1 properties
	vk::PhysicalDeviceVulkan12Properties                      vulkan12;                // Vulkan 1.2 properties
	vk::PhysicalDeviceVulkan13Properties                      vulkan13;                // Vulkan 1.3 properties
	vk::PhysicalDeviceGraphicsPipelineLibraryPropertiesEXT    graphics_pipeline_library; // Graphics pipeline library
};
} // namespace VB_NAMESPACE
#pragma once
