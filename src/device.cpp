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

#include "vulkan_backend/interface/device.hpp"
// #include "resource/device.hpp"
#include "resource/queue.hpp"

namespace VB_NAMESPACE {
void Device::UseDescriptor(Descriptor const& descriptor) {
	resource->descriptor = descriptor;
}

auto Device::GetBinding(DescriptorType const type) -> u32 {
	return resource->descriptor.GetBinding(type);
}

void Device::ResetStaging() {
	resource->stagingOffset = 0;
}

auto Device::GetStagingPtr() -> u8* {
	return resource->stagingCpu + resource->stagingOffset;
}

auto Device::GetStagingOffset() -> u32 {
	return resource->stagingOffset;
}

auto Device::GetStagingSize() -> u32 {
	return resource->init_info.staging_buffer_size;
}

auto Device::GetMaxSamples() -> SampleCount {
	return resource->physicalDevice->maxSamples;
}

void Device::WaitQueue(Queue const& queue) {
	auto result = vkQueueWaitIdle(queue.resource->handle);
	VB_CHECK_VK_RESULT(resource->owner->init_info.checkVkResult, result, "Failed to wait idle command buffer");
}

void Device::WaitIdle() {
	auto result = vkDeviceWaitIdle(resource->handle);
	VB_CHECK_VK_RESULT(resource->owner->init_info.checkVkResult, result, "Failed to wait idle command buffer");
}
auto Device::GetQueue(QueueInfo const& info) -> Queue {
	for (auto& q : resource->queuesResources) {
		if ((q.init_info.flags & info.flags) == info.flags &&
				(bool(info.supported_surface) || 
					q.init_info.supported_surface == info.supported_surface)) {
			Queue queue;
			queue.resource = &q;
			return queue;
		}
	}
	return Queue{};
}

auto Device::GetQueues() -> std::span<Queue> {
	return resource->queues;
}

auto Device::CreateBuffer(BufferInfo const& info) -> Buffer {
	return resource->CreateBuffer(info);
}

auto Device::CreateImage(ImageInfo const& info) -> Image {
	return resource->CreateImage(info);
}
}; // namespace VB_NAMESPACE