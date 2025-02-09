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

#ifndef VB_USE_VMA_MODULE
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif

#include <vulkan/vulkan.h>

#include "vulkan_backend/interface/buffer/buffer.hpp"
#include "vulkan_backend/interface/buffer/constants.hpp"
#include "vulkan_backend/interface/descriptor/descriptor.hpp"
#include "vulkan_backend/interface/device/device.hpp"
#include "vulkan_backend/interface/physical_device/physical_device.hpp"
#include "vulkan_backend/log.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/util/format.hpp"
#include "vulkan_backend/vk_result.hpp"

namespace VB_NAMESPACE {
Buffer::Buffer(Device& device, BufferInfo const& info) { Create(device, info); }

Buffer::Buffer(Buffer&& other) noexcept
	: vk::Buffer(std::exchange(static_cast<vk::Buffer&>(other), {})), ResourceBase(std::move(other)),
	  allocation(std::move(other.allocation)), allocation_info(std::move(other.allocation_info)), size(std::move(other.size)),
	  memory(std::move(other.memory)), usage(std::move(other.usage)) {}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
	if (this != &other) {
		vk::Buffer::operator=(std::exchange(static_cast<vk::Buffer&>(other), {}));
		ResourceBase::operator=(std::move(other));
		allocation      = std::move(other.allocation);
		allocation_info = std::move(other.allocation_info);
		size            = std::move(other.size);
		usage           = std::move(other.usage);
		memory          = std::move(other.memory);
	}
	return *this;
}

Buffer::~Buffer() { Free(); }

auto Buffer::GetSize() const -> u64 { return size; }

auto Buffer::GetMappedData() const -> void* {
	VB_ASSERT(memory & Memory::eCPU, "Buffer not cpu accessible!");
	return allocation_info.pMappedData;
}

auto Buffer::GetAddress() const -> vk::DeviceAddress { return GetDevice().getBufferAddress({.buffer = *this}); }

auto Buffer::Map() -> void* {
	VB_ASSERT(memory & Memory::eCPU, "Buffer not cpu accessible!");
	void* data;
	vmaMapMemory(GetDevice().GetVmaAllocator(), allocation, &data);
	return data;
}

void Buffer::Unmap() {
	VB_ASSERT(memory & Memory::eCPU, "Buffer not cpu accessible!");
	vmaUnmapMemory(GetDevice().GetVmaAllocator(), allocation);
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
		size += size % GetDevice().GetPhysicalDevice().GetProperties().GetCore10().limits.minStorageBufferOffsetAlignment;
	}

	if (usage & vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR) {
		usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
		usage |= vk::BufferUsageFlagBits::eTransferDst;
	}

	if (usage & vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR) {
		usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
	}
}

auto Buffer::Create(Device& device, BufferInfo const& info) -> vk::Result {
	ResourceBase::SetOwner(&device);
	SetName(info.name);
	this->size = info.create_info.size +
			   info.create_info.size %
				   GetDevice().GetPhysicalDevice().GetProperties().GetCore10().limits.minStorageBufferOffsetAlignment;
	this->usage = info.create_info.usage;
	this->memory = info.memory;

	vk::BufferCreateInfo bufferInfo{
		.size        = size,
		.usage       = usage,
		.sharingMode = vk::SharingMode::eExclusive,
	};

	VmaAllocationCreateInfo allocInfo = {
		.flags = memory & Memory::eCPU ? kBufferCpuFlags : 0,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	VB_LOG_TRACE("[ vmaCreateBuffer ] size = %zu, name = %s", bufferInfo.size, detail::FormatName(info.name).data());
	VB_VK_RESULT result =
		vk::Result(vmaCreateBuffer(GetDevice().GetVmaAllocator(), &reinterpret_cast<VkBufferCreateInfo&>(bufferInfo), &allocInfo,
								   reinterpret_cast<VkBuffer*>(static_cast<vk::Buffer*>(this)), &allocation, &allocation_info));
	VB_VERIFY_VK_RESULT(result, info.check_vk_results, "Failed to create buffer!", {});

	return result;
	// return vk::Result::eSuccess;
}

void Buffer::Free() {
	if (vk::Buffer::operator bool()) {
		VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), detail::FormatName((GetName())).data());
		vmaDestroyBuffer(GetDevice().GetVmaAllocator(), *this, allocation);
		vk::Buffer::operator=(vk::Buffer{});
	}
}

StagingBuffer::StagingBuffer(Device& device, vk::DeviceSize const size, std::string_view const name) {
	Create(device, size, name);
}

vb::StagingBuffer::StagingBuffer(StagingBuffer&& other) noexcept : Buffer(std::move(other)) { offset = std::move(other.offset); }

vb::StagingBuffer& vb::StagingBuffer::operator=(StagingBuffer&& other) noexcept {
	if (this != &other) {
		Buffer::operator=(std::move(other));
		offset = std::move(other.offset);
	}
	return *this;
}

auto StagingBuffer::Create(Device& device, vk::DeviceSize const size, std::string_view const name) -> vk::Result {
	VB_VERIFY_VK_RESULT(
		Buffer::Create(device,
					   {
						   .create_info = {.size = size, .usage = vk::BufferUsageFlagBits::eTransferSrc},
						   .memory      = Memory::eCPU,
						   .name        = name
    }),
		true, "Failed to create staging buffer!", {});
	offset = 0;
	return vk::Result::eSuccess;
}

void StagingBuffer::Reset() { offset = 0; }

auto StagingBuffer::GetPtr() const -> u8* { return reinterpret_cast<u8*>(GetMappedData()) + offset; }

auto StagingBuffer::GetOffset() const -> u32 { return offset; }

BindlessBuffer::BindlessBuffer(Device& device, BindlessDescriptor& descriptor, BindlessBufferInfo const& info) {
	Create(device, descriptor, info);
}

BindlessBuffer::BindlessBuffer(BindlessBuffer&& other) noexcept
	: Buffer(std::move(other)), BindlessResourceBase(std::move(other)) {}

BindlessBuffer& BindlessBuffer::operator=(BindlessBuffer&& other) noexcept {
	if (this != &other) {
		Buffer::operator=(std::move(other));
		BindlessResourceBase::operator=(std::move(other));
	}
	return *this;
}

BindlessBuffer::~BindlessBuffer() { Free(); }

auto BindlessBuffer::Create(Device& device, BindlessDescriptor& descriptor, BindlessBufferInfo const& info) -> vk::Result {
	VB_VERIFY_VK_RESULT(Buffer::Create(device, info.buffer_info), info.buffer_info.check_vk_results, "Failed to create bindless buffer!", {});

	// Bind to get resource id
	BindlessResourceBase::Bind(descriptor, info.binding);

	// Update bindless descriptor
	vk::DescriptorBufferInfo bufferInfo = {
		.buffer = *this,
		.offset = 0,
		.range  = info.buffer_info.create_info.size,
	};

	vk::WriteDescriptorSet write = {
		.dstSet          = descriptor.GetSet(),
		.dstBinding      = info.binding,
		.dstArrayElement = GetResourceID(),
		.descriptorCount = 1,
		.descriptorType  = descriptor.GetBindingInfo(info.binding).descriptorType,
		.pBufferInfo     = &bufferInfo,
	};

	GetDevice().updateDescriptorSets(1, &write, 0, nullptr);
	return vk::Result::eSuccess;
}

void BindlessBuffer::Free() {
	if (BindlessResourceBase::IsBound()) {
		BindlessResourceBase::Release();
	}
	Buffer::Free();
}

} // namespace VB_NAMESPACE
