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
#include "vulkan_backend/util/bits.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
namespace util {
inline auto DynamicStateInfo(std::span<const vk::DynamicState> dynamic_states) -> vk::PipelineDynamicStateCreateInfo{
	return {
		.dynamicStateCount = static_cast<u32>(dynamic_states.size()),
		.pDynamicStates    = dynamic_states.data(),
	};
} // GetDynami
}

constexpr inline auto AspectFromFormat(vk::Format const f) -> vk::ImageAspectFlags {
	switch (f) {
	case vk::Format::eD24UnormS8Uint: // Invalid, cannot be both stencil and depth,
										// todo: create separate imageview
		return vk::ImageAspectFlags(vk::ImageAspectFlagBits::eDepth |
									vk::ImageAspectFlagBits::eStencil);
	case vk::Format::eD32Sfloat:
	case vk::Format::eD16Unorm:
		return vk::ImageAspectFlagBits::eDepth;
	default:
		return vk::ImageAspectFlagBits::eColor;
	}
};

constexpr inline auto ImageLayout(vk::ImageUsageFlags const usage, vk::ImageAspectFlags const aspect) -> vk::ImageLayout {
	vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal;
	if (usage & vk::ImageUsageFlagBits::eSampled) {

		if (TestBits(aspect, vk::ImageAspectFlagBits::eDepth)) [[unlikely]] {
			layout = vk::ImageLayout::eDepthReadOnlyOptimal;
		}

		if (TestBits(aspect, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil)) [[unlikely]] {
			layout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
		}
	}
	if (usage & vk::ImageUsageFlagBits::eStorage) {
		layout = vk::ImageLayout::eGeneral;
	}

	return layout;
}
} // namespace VB_NAMESPACE
