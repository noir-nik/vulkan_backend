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
// struct BindingInfo {
// 	vk::DescriptorSetLayoutBinding binding;
// 	vk::DescriptorBindingFlags flags = vk::DescriptorBindingFlagBits::eUpdateAfterBind;
// };

// struct BindlessDescriptorInfo {
// 	std::span<BindingInfo const> bindings;
// 	vk::DescriptorPoolCreateFlags pool_flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
// };

struct DescriptorInfo {
	std::span<vk::DescriptorSetLayoutBinding const> bindings;
	// If binding_flags.size() < bindings.size(), the rest will be filled with zero
	std::span<vk::DescriptorBindingFlags const> binding_flags;
	vk::DescriptorPoolCreateFlags               pool_flags;
	vk::DescriptorSetLayoutCreateFlags          layout_flags;
	bool check_vk_results = true;
};
} // namespace VB_NAMESPACE
