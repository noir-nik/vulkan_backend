#ifndef VULKAN_BACKEND_RESOURCE_BUFFER_HPP_
#define VULKAN_BACKEND_RESOURCE_BUFFER_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#else
import std;
#endif

#include <vulkan/vulkan.h>

#include <vulkan_backend/fwd.hpp>
#include "vulkan_backend/structs.hpp"
#include "base.hpp"
#include "device.hpp"
#include "descriptor.hpp"
#include "../util.hpp"

namespace VB_NAMESPACE {
struct BufferResource : ResourceBase<DeviceResource> {
	u32 rid = NullRID;

	VkBuffer handle;
	VmaAllocation allocation;

	u64 size;
	BufferUsageFlags usage;
	MemoryFlags memory;

	using ResourceBase::ResourceBase;

	inline auto GetInstance() -> InstanceResource* { return owner->owner; }
	auto GetType() -> char const* override {  return "BufferResource"; }
	void Free() override {
		VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
		vmaDestroyBuffer(owner->vmaAllocator, handle, allocation);
		if (rid != NullRID) {
			owner->descriptor.resource->PushID(DescriptorType::eStorageBuffer, rid);
		}
	}
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_RESOURCE_BUFFER_HPP_