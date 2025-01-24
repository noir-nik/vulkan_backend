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
constexpr inline char const* const kValidationLayerName = "VK_LAYER_KHRONOS_validation";

constexpr inline vk::PhysicalDeviceVulkan12Features kRequiredVulkan12Features{
	// descriptor indexing
	.shaderUniformBufferArrayNonUniformIndexing	   = true,
	.shaderSampledImageArrayNonUniformIndexing	   = true,
	.shaderStorageBufferArrayNonUniformIndexing	   = true,
	.descriptorBindingSampledImageUpdateAfterBind  = true,
	.descriptorBindingStorageImageUpdateAfterBind  = true,
	.descriptorBindingStorageBufferUpdateAfterBind = true,
	.descriptorBindingPartiallyBound			   = true,
	.runtimeDescriptorArray						   = true,
	// buffer device address
	.bufferDeviceAddress = true,
};

constexpr inline vk::PhysicalDeviceVulkan13Features kRequiredVulkan13Features{
	.synchronization2 = true,
	.dynamicRendering = true,
};

namespace defaults {

} // namespace defaults
} // namespace VB_NAMESPACE
