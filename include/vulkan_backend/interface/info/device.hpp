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

#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/types.hpp"
#include "vulkan_backend/interface/info/queue.hpp"
#include "vulkan_backend/classes/structure_chain.hpp"
#include "vulkan_backend/interface/info/descriptor.hpp"
#include "vulkan_backend/defaults/descriptor.hpp"
#include "vulkan_backend/defaults/device.hpp"


VB_EXPORT
namespace VB_NAMESPACE {
struct DeviceInfo {	
	// Queues to create with device
	// By default one graphics queue is created
	std::span<QueueInfo const> queues = defaults::kOneComputeQueue;

	// Staging buffer that lives with device
	// By default staging buffer size is 64MB
	// Can be set to 0 to disable staging buffer creation
	u32 staging_buffer_size = defaults::kStagingSize;

	// Bindless descriptor bindings and flags
	DescriptorInfo const& descriptor_info = defaults::kBindlessDescriptorInfo;

	// Extensions to enable
	std::span<char const* const> extensions = {};

	// Additional extensions (enabled if supported by selected device)
	std::span<char const* const> optional_extensions = defaults::kOptionalDeviceExtensions;

	// Features to enable when creating device
	// Their support is NOT be checked before device creation
	// It is possible to pass vk::PhysicalDeviceFeatures2 part from vk::StructureChain
	// Some functions are provided to chain structures:
	// - SetupStructureChain()
	// - AppendStructureChain()
	// - InsertStructureAfter()
	vk::PhysicalDeviceFeatures2 const& feature_chain = kRequiredFeatures.features2;

	// Give a name to device or use name of respective physical device
	std::string_view name = "";
};

} // namespace VB_NAMESPACE
