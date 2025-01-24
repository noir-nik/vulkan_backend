#pragma once

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"
#include "vulkan_backend/types.hpp"
#include "vulkan_backend/interface/info/descriptor.hpp"

VB_EXPORT
namespace VB_NAMESPACE {	
namespace defaults {
u32 constexpr inline kMaxDescriptors = 8192;
u32 constexpr inline kBindingCombinedImageSampler = 0;
u32 constexpr inline kBindingStorageBuffer = 1;
u32 constexpr inline kBindingStorageImage = 2;

vk::DescriptorSetLayoutBinding constexpr inline kBindings[] = {
	{kBindingCombinedImageSampler, vk::DescriptorType::eCombinedImageSampler, kMaxDescriptors, vk::ShaderStageFlagBits::eAll},
	{kBindingStorageBuffer, vk::DescriptorType::eStorageBuffer, kMaxDescriptors, vk::ShaderStageFlagBits::eAll},
	{kBindingStorageImage, vk::DescriptorType::eStorageImage, kMaxDescriptors, vk::ShaderStageFlagBits::eAll},
};

vk::DescriptorBindingFlags constexpr inline kBindingFlags[] = {
	vk::DescriptorBindingFlagBits::eUpdateAfterBind,
	vk::DescriptorBindingFlagBits::eUpdateAfterBind,
	vk::DescriptorBindingFlagBits::eUpdateAfterBind,
};

DescriptorInfo constexpr inline kBindlessDescriptorInfo = {
	.bindings = kBindings,
	.binding_flags = kBindingFlags,
	.pool_flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
	.layout_flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
};
} // namespace defaults
} // namespace VB_NAMESPACE
