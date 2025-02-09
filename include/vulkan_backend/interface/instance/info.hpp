#pragma once

#ifndef VB_USE_STD_MODULE
#include <span>
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
struct InstanceCreateInfo {
	// Required layers
	std::span<char const* const> required_layers = {};
	
	// Required instance extensions
	std::span<char const* const> required_extensions = {};
	
	// Optional layers, for example vb::kValidationLayerName
	std::span<char const* const> optional_layers = {};
	
	// Optional instance extensions, for example vk::EXTDebugUtilsExtensionName
	std::span<char const* const> optional_extensions = {};
	
	// Optional data, provides useful information to driver
	vk::ApplicationInfo const& application_info = {.apiVersion = vk::ApiVersion13};

	// Optional allocator
	vk::AllocationCallbacks* allocator = nullptr;
	
	bool check_vk_results = true;
};

} // namespace VB_NAMESPACE
