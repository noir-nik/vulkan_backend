#ifndef VB_USE_STD_MODULE
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/interface/queue/queue.hpp"
#include "vulkan_backend/interface/device/device.hpp"
#include "vulkan_backend/vk_result.hpp"

namespace VB_NAMESPACE {
Queue::Queue(vk::Queue queue, Device& device, u32 family, u32 index, vk::QueueFlags flags)
	: vk::Queue(queue), device(&device), family(family), index(index), flags(flags) {}

void Queue::Submit(
		std::span<vk::CommandBufferSubmitInfo const> cmds,
		vk::Fence fence,
		SubmitInfo const& info) const {

	vk::SubmitInfo2 submitInfo {
		.waitSemaphoreInfoCount = static_cast<u32>(info.waitSemaphoreInfos.size()),
		.pWaitSemaphoreInfos = info.waitSemaphoreInfos.data(),
		.commandBufferInfoCount = static_cast<u32>(cmds.size()),
		.pCommandBufferInfos = cmds.data(),
		.signalSemaphoreInfoCount = static_cast<u32>(info.signalSemaphoreInfos.size()),
		.pSignalSemaphoreInfos = info.signalSemaphoreInfos.data(),
	};

	auto result = submit2(submitInfo, fence);
	VB_CHECK_VK_RESULT(result, "Failed to submit command buffer");
}

void Queue::Wait() const {
	auto result = waitIdle();
	VB_CHECK_VK_RESULT(result, "Failed to wait queue idle");
}


auto Queue::GetFamilyIndex() const -> u32 {
	return family;
}
auto Queue::GetIndex() const -> u32 {
	return index;
}

auto Queue::GetFlags() const -> vk::QueueFlags {
	return flags;
}


} // namespace VB_NAMESPACE