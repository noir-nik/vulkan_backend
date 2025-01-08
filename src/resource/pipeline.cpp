#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <algorithm>
#else
import std;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include <vulkan/vulkan.h>

#include "pipeline.hpp"
#include "device.hpp"
#include "instance.hpp"
#include "../macros.hpp"

namespace VB_NAMESPACE {
void PipelineResource::CreateLayout(std::span<VkDescriptorSetLayout const> descriptorLayouts) {
	VkPushConstantRange pushConstant{
		.stageFlags = VK_SHADER_STAGE_ALL,
		.offset     = 0,
		.size       = owner->physicalDevice->physicalProperties2.properties.limits.maxPushConstantsSize,
	};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount         = static_cast<u32>(descriptorLayouts.size()),
		.pSetLayouts            = descriptorLayouts.data(),
		.pushConstantRangeCount = 1,
		.pPushConstantRanges    = &pushConstant,
	};

	VB_VK_RESULT result = vkCreatePipelineLayout(owner->handle, &pipelineLayoutInfo, owner->owner->allocator, &this->layout);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create pipeline layout!");
}

auto PipelineResource::GetInstance() const -> InstanceResource* {
	return owner->owner;
}

void PipelineResource::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
	vkDestroyPipeline(owner->handle, handle, owner->owner->allocator);
	vkDestroyPipelineLayout(owner->handle, layout, owner->owner->allocator);
}
}; // namespace VB_NAMESPACE