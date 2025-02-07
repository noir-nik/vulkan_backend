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

#include "vulkan_backend/interface/buffer/buffer.hpp"
#include "vulkan_backend/interface/descriptor/descriptor.hpp"
#include "vulkan_backend/interface/device/device.hpp"
#include "vulkan_backend/interface/physical_device/physical_device.hpp"
#include "vulkan_backend/interface/buffer/constants.hpp"
#include "vulkan_backend/log.hpp"
#include "vulkan_backend/vk_result.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/util/format.hpp"
#include "vulkan_backend/util/bits.hpp"

namespace VB_NAMESPACE {
Buffer::Buffer(Device& device, BufferInfo const& info)
	: ResourceBase(&device) {
	Create(device, info);
}

Buffer::Buffer(Buffer&& other) noexcept
	: ResourceBase(std::move(other)),
	  allocation(other.allocation),
	  allocation_info(other.allocation_info),
	  info(std::move(other.info)) {
	other.allocation = {};
	other.allocation_info = {};
	other.info = {};
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
	if (this != &other) {
		ResourceBase::operator=(std::move(other));
		allocation = other.allocation;
		allocation_info = other.allocation_info;
		info = std::move(other.info);
		other.allocation = {};
		other.allocation_info = {};
		other.info = {};
	}
	return *this;
}


Buffer::~Buffer() { Free(); }

auto Buffer::GetSize() const -> u64 { return info.size; }

auto Buffer::GetMappedData() const -> void* {
	VB_ASSERT(info.memory & Memory::eCPU, "Buffer not cpu accessible!");
	return allocation_info.pMappedData;
}

auto Buffer::GetAddress() const -> vk::DeviceAddress {
	return owner->getBufferAddress({.buffer = *this});
}

auto Buffer::Map() -> void* {
	VB_ASSERT(info.memory & Memory::eCPU, "Buffer not cpu accessible!");
	void* data;
	vmaMapMemory(owner->vma_allocator, allocation, &data);
	return data;
}

void Buffer::Unmap() {
	VB_ASSERT(info.memory & Memory::eCPU, "Buffer not cpu accessible!");
	vmaUnmapMemory(owner->vma_allocator, allocation);
}

auto Buffer::GetResourceTypeName() const -> char const* { return "BufferResource"; }

void Buffer::AddUsageFlags(vk::BufferUsageFlags& usage, u64& size) {
	if (usage & vk::BufferUsageFlagBits::eVertexBuffer) {
		usage |= vk::BufferUsageFlagBits::eTransferDst;
	}

	if (usage & vk::BufferUsageFlagBits::eIndexBuffer) {
		usage |= vk::BufferUsageFlagBits::eTransferDst;
	}

	if (usage & vk::BufferUsageFlagBits::eStorageBuffer) {
		usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
		size += size % owner->physical_device->GetProperties2()
						   .properties.limits.minStorageBufferOffsetAlignment;
	}

	if (usage & vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR) {
		usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
		usage |= vk::BufferUsageFlagBits::eTransferDst;
	}

	if (usage & vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR) {
		usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
	}
}

void Buffer::Create(Device& device, BufferInfo const& info) {
	this->owner = &device;
	this->info = info;
	SetName(info.name);
	u32 size = info.size +
			   info.size %
				   owner->physical_device->GetProperties2().properties.limits.minStorageBufferOffsetAlignment;
	vk::BufferUsageFlags usage = info.usage;

	vk::BufferCreateInfo bufferInfo{
		.size		 = size,
		.usage		 = usage,
		.sharingMode = vk::SharingMode::eExclusive,
	};

	VmaAllocationCreateInfo allocInfo = {
		.flags = info.memory & Memory::eCPU ? kBufferCpuFlags : 0,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	VB_LOG_TRACE("[ vmaCreateBuffer ] size = %zu, name = %s", bufferInfo.size, detail::FormatName(info.name).data());
	VB_VK_RESULT result = vk::Result(vmaCreateBuffer(
		owner->vma_allocator, &reinterpret_cast<VkBufferCreateInfo&>(bufferInfo), &allocInfo,
		reinterpret_cast<VkBuffer*>(static_cast<vk::Buffer*>(this)), &allocation, &allocation_info));
	VB_CHECK_VK_RESULT(result, "Failed to create buffer!");

	// Update bindless descriptor
	if (info.descriptor != nullptr) {
		VB_ASSERT(info.memory == Memory::eGPU,
				  "Cannot write descriptor for buffer with cpu accessible memory");
		SetResourceID(info.descriptor->PopID(info.binding));

		vk::DescriptorBufferInfo bufferInfo = {
			.buffer = *this,
			.offset = 0,
			.range	= size,
		};

		vk::WriteDescriptorSet write = {
			.dstSet			 = info.descriptor->set,
			.dstBinding		 = info.binding,
			.dstArrayElement = GetResourceID(),
			.descriptorCount = 1,
			.descriptorType	 = info.descriptor->GetBindingInfo(info.binding).descriptorType,
			.pBufferInfo	 = &bufferInfo,
		};

		owner->updateDescriptorSets(1, &write, 0, nullptr);
	} else {
		if (TestBits(info.usage, vk::BufferUsageFlagBits::eStorageBuffer) &&
			!TestBits(info.usage, vk::BufferUsageFlagBits::eShaderDeviceAddress)) {
			VB_LOG_WARN("Storage buffer created without descriptor write or shader device address");
		}
	}
}

void Buffer::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), detail::FormatName((GetName())).data());
	vmaDestroyBuffer(owner->vma_allocator, *this, allocation);
	if (GetResourceID() != kNullID) {
		info.descriptor->PushID(GetBinding(), GetResourceID());
	}
}

StagingBuffer::StagingBuffer(Device&  device, u32 size, std::string_view name)
	: Buffer(device, {size, vk::BufferUsageFlagBits::eTransferSrc, Memory::eCPU, name}) {
	offset = 0;
}

void StagingBuffer::Reset() { offset = 0; }

auto StagingBuffer::GetPtr() const-> u8* { return reinterpret_cast<u8*>(GetMappedData()) + offset; }

auto StagingBuffer::GetOffset() const-> u32 { return offset; }

} // namespace VB_NAMESPACE