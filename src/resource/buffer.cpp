#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <cstdint>
#else
import std;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "buffer.hpp"
#include "instance.hpp"
#include "device.hpp"
#include "descriptor.hpp"

namespace VB_NAMESPACE {
auto Buffer::GetResourceID() const -> u32 {
	VB_ASSERT(resource->rid != kNullResourceID, "Invalid buffer rid");
	return resource->rid;
}

auto Buffer::GetSize() const -> u64 {
	return resource->size;
}

auto Buffer::GetMappedData() -> void* {
	VB_ASSERT(resource->memory & Memory::CPU, "Buffer not cpu accessible!");
	return resource->allocationInfo.pMappedData;
}

auto Buffer::GetAddress() -> DeviceAddress {
	VkBufferDeviceAddressInfo info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = resource->handle,
	};
	return vkGetBufferDeviceAddress(resource->owner->handle, &info);
}

auto Buffer::Map() -> void* {
	VB_ASSERT(resource->memory & Memory::CPU, "Buffer not cpu accessible!");
	void* data;
	vmaMapMemory(resource->owner->vmaAllocator, resource->allocation, &data);
	return data;
}

void Buffer::Unmap() {
	VB_ASSERT(resource->memory & Memory::CPU, "Buffer not cpu accessible!");
	vmaUnmapMemory(resource->owner->vmaAllocator, resource->allocation);
}

auto BufferResource::GetType() const -> char const* { return "BufferResource"; }

auto BufferResource::GetInstance() const -> InstanceResource* { return owner->owner; }

void BufferResource::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
	vmaDestroyBuffer(owner->vmaAllocator, handle, allocation);
	if (rid != kNullResourceID) {
		owner->descriptor.resource->PushID(DescriptorType::eStorageBuffer, rid);
	}
}
} // namespace VB_NAMESPACE