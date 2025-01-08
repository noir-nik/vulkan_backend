#ifndef VULKAN_BACKEND_RESOURCE_COMMAND_HPP_
#define VULKAN_BACKEND_RESOURCE_COMMAND_HPP_

#include <vulkan/vulkan.h>

#include "vulkan_backend/fwd.hpp"
#include "base.hpp"

namespace VB_NAMESPACE {
struct CommandResource : ResourceBase<DeviceResource> {
	VkCommandBuffer handle  = VK_NULL_HANDLE;
	VkCommandPool   pool    = VK_NULL_HANDLE;
	VkFence         fence   = VK_NULL_HANDLE;

	using ResourceBase::ResourceBase;

	void Create(u32 queueFamilyindex);

	auto GetType() const-> char const* override;
	auto GetInstance() const -> InstanceResource* override;
	void Free() override;
};
} // namespace VB_NAMESPACE
#endif // VULKAN_BACKEND_RESOURCE_COMMAND_HPP_