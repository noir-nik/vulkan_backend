#ifndef VULKAN_BACKEND_RESOURCE_BUFFER_HPP_
#define VULKAN_BACKEND_RESOURCE_BUFFER_HPP_

#include <vulkan/vulkan.h>

#include <vk_mem_alloc.h>

#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/structs.hpp"
#include "base.hpp"

namespace VB_NAMESPACE {
struct BufferResource : ResourceBase<DeviceResource> {
	u32 rid = kNullResourceID;

	VkBuffer handle;
	VmaAllocation allocation;
	VmaAllocationInfo allocationInfo;

	u64 size;
	BufferUsageFlags usage;
	MemoryFlags memory;

	using ResourceBase::ResourceBase;

	auto GetType() const -> char const* override;
	auto GetInstance() const -> InstanceResource* override;
	void Free() override;
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_RESOURCE_BUFFER_HPP_