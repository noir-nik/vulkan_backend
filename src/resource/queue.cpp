#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#else
import std;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/interface/queue.hpp"
#include "queue.hpp"
#include "device.hpp"
#include "instance.hpp"
#include "../macros.hpp"

namespace VB_NAMESPACE {
void Queue::Submit(
		std::span<CommandBufferSubmitInfo const> cmds,
		Fence fence,
		SubmitInfo const& info) const {

	VkSubmitInfo2 submitInfo {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount = static_cast<u32>(info.waitSemaphoreInfos.size()),
		.pWaitSemaphoreInfos = reinterpret_cast<VkSemaphoreSubmitInfo const*>(info.waitSemaphoreInfos.data()),
		.commandBufferInfoCount = static_cast<u32>(cmds.size()),
		.pCommandBufferInfos = reinterpret_cast<VkCommandBufferSubmitInfo const*>(cmds.data()),
		.signalSemaphoreInfoCount = static_cast<u32>(info.signalSemaphoreInfos.size()),
		.pSignalSemaphoreInfos = reinterpret_cast<VkSemaphoreSubmitInfo const*>(info.signalSemaphoreInfos.data()),
	};

	auto result = vkQueueSubmit2(resource->handle, 1, &submitInfo, fence);
	VB_CHECK_VK_RESULT(resource->device->owner->init_info.checkVkResult, result, "Failed to submit command buffer");
}

auto Queue::GetInfo() const -> QueueInfo {
	return resource->init_info;
}

auto Queue::GetFamilyIndex() const -> u32 {
	return resource->familyIndex;
}

auto Queue::GetHandle() const -> vk::Queue {
	return resource->handle;
}

} // namespace VB_NAMESPACE