#pragma once

#ifndef VB_USE_STD_MODULE
#include <string_view>
#include <optional>
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
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/defaults/pipeline.hpp"

VB_EXPORT
namespace VB_NAMESPACE {

struct PipelineLayoutInfo {
	std::span<vk::DescriptorSetLayout const> descriptor_set_layouts;
	std::span<vk::PushConstantRange const>   push_constant_ranges;
	bool check_vk_results = true;
};

} // namespace VB_NAMESPACE
