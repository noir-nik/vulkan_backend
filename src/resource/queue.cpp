#ifndef VB_USE_STD_MODULE
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/interface/queue.hpp"
#include "vulkan_backend/interface/device.hpp"
#include "vulkan_backend/interface/instance.hpp"
#include "vulkan_backend/vk_result.hpp"
#include "vulkan_backend/macros.hpp"

namespace VB_NAMESPACE {
Queue::Queue(vk::Queue queue, Device* device, u32 family, u32 index, QueueInfo info)
	: vk::Queue(queue), device(device), family(family), index(index), info(info) {}

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

auto Queue::GetInfo() const -> QueueInfo {
	return info;
}

auto Queue::GetFamilyIndex() const -> u32 {
	return family;
}
auto Queue::GetIndex() const -> u32 {
	return index;
}


} // namespace VB_NAMESPACE