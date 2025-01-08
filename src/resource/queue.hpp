#ifndef VULKAN_BACKEND_RESOURCE_DESCRIPTOR_HPP_
#define VULKAN_BACKEND_RESOURCE_DESCRIPTOR_HPP_

#include <vulkan/vulkan.h>

#include <vulkan_backend/fwd.hpp>
#include "vulkan_backend/interface/queue.hpp"
#include "common.hpp"

namespace VB_NAMESPACE {
struct QueueResource : NoCopy {
	DeviceResource* device = nullptr;
	u32 familyIndex = ~0;
	u32 index = 0;
	QueueInfo init_info;
	VkQueue handle = VK_NULL_HANDLE;
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_RESOURCE_DESCRIPTOR_HPP_