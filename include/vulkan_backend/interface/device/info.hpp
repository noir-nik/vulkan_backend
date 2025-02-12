#pragma once

#ifndef VB_USE_STD_MODULE
#include <span>
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

VB_EXPORT
namespace VB_NAMESPACE {
struct DeviceInfo {	
	// Queues to create with device.
	// Leave pQueuePriorities = nullptr to use 1.0f for all queues
	// Fill only:
	// .queueFamilyIndex
	// .queueCount
	std::span<vk::DeviceQueueCreateInfo const> queues;
	
	// Extensions to enable like:
	// vk::KHRSwapchainExtensionName
	std::span<char const* const> extensions = {};

	// Additional extensions (enabled if supported by selected device)
	std::span<char const* const> optional_extensions;

	// Features to enable when creating device
	// Their support is NOT be checked before device creation
	// It is possible to pass vk::PhysicalDeviceFeatures2 part from vk::StructureChain
	// Some functions are provided to chain structures:
	// - SetupStructureChain()
	// - AppendStructureChain()
	// - InsertStructureAfter()
	vk::PhysicalDeviceFeatures2 const* const features2 = nullptr;

	// Give a name to device or use name of respective physical device
	std::string_view const name = "";
	bool check_vk_results = true;
};

} // namespace VB_NAMESPACE
