#ifndef VB_USE_STD_MODULE
#include <cstdint>
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#ifndef VB_DEV
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif

#include <vulkan/vulkan.h>

#include "vulkan_backend/interface/buffer.hpp"
#include "vulkan_backend/interface/descriptor.hpp"
#include "vulkan_backend/interface/device.hpp"
#include "vulkan_backend/log.hpp"
#include "vulkan_backend/vk_result.hpp"
#include "vulkan_backend/macros.hpp"


namespace VB_NAMESPACE {
Buffer::Buffer(std::shared_ptr<Device> const& device, BufferInfo const& info)
	: ResourceBase(device) {
	Create(device.get(), info);
}

Buffer::~Buffer() { Free(); }

auto Buffer::GetSize() const -> u64 { return info.size; }

auto Buffer::GetMappedData() -> void* {
	VB_ASSERT(info.memory & Memory::CPU, "Buffer not cpu accessible!");
	return allocationInfo.pMappedData;
}

auto Buffer::GetAddress() const -> vk::DeviceAddress {
	return device->getBufferAddress({.buffer = *this});
}

auto Buffer::Map() -> void* {
	VB_ASSERT(info.memory & Memory::CPU, "Buffer not cpu accessible!");
	void* data;
	vmaMapMemory(device->vma_allocator, allocation, &data);
	return data;
}

void Buffer::Unmap() {
	VB_ASSERT(info.memory & Memory::CPU, "Buffer not cpu accessible!");
	vmaUnmapMemory(device->vma_allocator, allocation);
}

auto Buffer::GetResourceTypeName() const -> char const* { return "BufferResource"; }

void Buffer::Create(Device* device, BufferInfo const& info) {
	this->device = device;
	this->info = info;
	SetName(info.name);
	vk::BufferUsageFlags usage = info.usage;
	u32					 size  = info.size;

	if (usage & vk::BufferUsageFlagBits::eVertexBuffer) {
		usage |= vk::BufferUsageFlagBits::eTransferDst;
	}

	if (usage & vk::BufferUsageFlagBits::eIndexBuffer) {
		usage |= vk::BufferUsageFlagBits::eTransferDst;
	}

	if (usage & vk::BufferUsageFlagBits::eStorageBuffer) {
		usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
		size += size % device->physical_device->GetProperties2()
						   .properties.limits.minStorageBufferOffsetAlignment;
	}

	if (usage & vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR) {
		usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
		usage |= vk::BufferUsageFlagBits::eTransferDst;
	}

	if (usage & vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR) {
		usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
	}

	vk::BufferCreateInfo bufferInfo{
		.size		 = size,
		.usage		 = usage,
		.sharingMode = vk::SharingMode::eExclusive,
	};

	VmaAllocationCreateFlags constexpr cpuFlags = {
		VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT};

	VmaAllocationCreateInfo allocInfo = {
		.flags = info.memory & Memory::CPU ? cpuFlags : 0,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	VB_LOG_TRACE("[ vmaCreateBuffer ] name = %s, size = %zu", info.name.data(), bufferInfo.size);
	VB_VK_RESULT result = vk::Result(vmaCreateBuffer(
		device->vma_allocator, &reinterpret_cast<VkBufferCreateInfo&>(bufferInfo), &allocInfo,
		reinterpret_cast<VkBuffer*>(static_cast<vk::Buffer*>(this)), &allocation, &allocationInfo));
	VB_CHECK_VK_RESULT(result, "Failed to create buffer!");

	// Update bindless descriptor
	if (info.binding != BufferInfo::kBindingNone) {
		VB_ASSERT(info.memory == Memory::GPU,
				  "Cannot write descriptor for buffer with cpu accessible memory");
		SetResourceID(device->descriptor.PopID(info.binding));

		vk::DescriptorBufferInfo bufferInfo = {
			.buffer = *this,
			.offset = 0,
			.range	= size,
		};

		vk::WriteDescriptorSet write = {
			.dstSet			 = device->descriptor.set,
			.dstBinding		 = info.binding,
			.dstArrayElement = GetResourceID(),
			.descriptorCount = 1,
			.descriptorType	 = device->descriptor.GetBindingInfo(info.binding).descriptorType,
			.pBufferInfo	 = &bufferInfo,
		};

		device->updateDescriptorSets(1, &write, 0, nullptr);
	}
}

void Buffer::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), GetName());
	vmaDestroyBuffer(device->vma_allocator, *this, allocation);
	if (GetResourceID() != kNullID) {
		device->descriptor.PushID(GetBinding(), GetResourceID());
	}
}
} // namespace VB_NAMESPACE