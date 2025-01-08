#ifndef VULKAN_BACKEND_RESOURCE_PIPELINE_HPP_
#define VULKAN_BACKEND_RESOURCE_PIPELINE_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#else
import std;
#endif

#include <vulkan/vulkan.h>

#include <vulkan_backend/fwd.hpp>
#include "base.hpp"
#include "device.hpp"
#include "../util.hpp"

namespace VB_NAMESPACE {
struct PipelineResource : ResourceBase<DeviceResource> {
	VkPipeline handle;
	VkPipelineLayout layout;
	PipelinePoint point;

	using ResourceBase::ResourceBase;

	inline void CreateLayout(std::span<VkDescriptorSetLayout const> descriptorLayouts);
	
	inline auto GetInstance() -> InstanceResource* { return owner->owner; }
	auto GetType() -> char const* override {  return "PipelineResource"; }
	void Free() override {
		VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
		vkDestroyPipeline(owner->handle, handle, owner->owner->allocator);
		vkDestroyPipelineLayout(owner->handle, layout, owner->owner->allocator);
	}
};

inline void PipelineResource::CreateLayout(std::span<VkDescriptorSetLayout const> descriptorLayouts) {
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
} // namespace VB_NAMESPACE


#endif // VULKAN_BACKEND_RESOURCE_PIPELINE_HPP_