#ifndef VULKAN_BACKEND_RESOURCE_COMMAND_HPP_
#define VULKAN_BACKEND_RESOURCE_COMMAND_HPP_

#include <vulkan/vulkan.h>

#include <vulkan_backend/fwd.hpp>
#include "base.hpp"
#include "device.hpp"
#include "../util.hpp"

namespace VB_NAMESPACE {
struct CommandResource : ResourceBase<DeviceResource> {
	VkCommandBuffer handle = VK_NULL_HANDLE;
	VkCommandPool   pool  = VK_NULL_HANDLE;
	VkFence         fence = VK_NULL_HANDLE;

	using ResourceBase::ResourceBase;

	inline void Create(u32 queueFamilyindex);
	inline auto GetInstance() -> InstanceResource* { return owner->owner; }
	auto GetType() -> char const* override { return "CommandResource"; }
	void Free() override {
		VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
		vkDestroyCommandPool(owner->handle, pool, owner->owner->allocator);
		vkDestroyFence(owner->handle, fence, owner->owner->allocator);
	}
};

inline void CommandResource::Create(u32 queueFamilyindex) {
	VkCommandPoolCreateInfo poolInfo {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = 0, // do not use VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
		.queueFamilyIndex = queueFamilyindex
	};

	VkCommandBufferAllocateInfo allocInfo {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	VB_VK_RESULT result = vkCreateCommandPool(owner->handle, &poolInfo, owner->owner->allocator, &pool);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create command pool!");

	allocInfo.commandPool = pool;
	result = vkAllocateCommandBuffers(owner->handle, &allocInfo, &handle);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to allocate command buffer!");

	VkFenceCreateInfo fenceInfo{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	result = vkCreateFence(owner->handle, &fenceInfo, owner->owner->allocator, &fence);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create fence!");
}

} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_RESOURCE_COMMAND_HPP_