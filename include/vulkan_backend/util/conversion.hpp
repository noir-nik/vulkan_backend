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
#include "vulkan_backend/types.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
namespace convert {
inline auto DynamicStateInfo(std::span<const vk::DynamicState> dynamic_states) -> vk::PipelineDynamicStateCreateInfo{
	return {
		.dynamicStateCount = static_cast<u32>(dynamic_states.size()),
		.pDynamicStates    = dynamic_states.data(),
	};
} // GetDynami
}

} // namespace VB_NAMESPACE
