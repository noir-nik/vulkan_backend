#pragma once

#ifndef VB_USE_STD_MODULE
#include <string_view>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#ifndef VB_USE_VMA_MODULE
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif

#include "vulkan_backend/classes/base.hpp"
#include "vulkan_backend/classes/gpu_resource.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/buffer/info.hpp"
#include "vulkan_backend/types.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
class Buffer : public vk::Buffer, public Named, public ResourceBase<Device> {
  public:
	// No-op constructor
	Buffer() = default;

	// RAII constructor, calls Create
	Buffer(Device& device, BufferInfo const& info);

	// Move constructor
	Buffer(Buffer&& other) noexcept;

	// Move assignment
	Buffer& operator=(Buffer&& other) noexcept;

	// Destructor, frees resources
	~Buffer();

	// Create with result checked
	auto Create(Device& device, BufferInfo const& info) -> vk::Result;

	// Frees all resources
	void Free() override;

	// Get actual size after alignment
	auto GetSize() const -> u64;

	// Get Device address
	auto GetAddress() const -> vk::DeviceAddress;

	// Get mapped data pointer (Only CPU)
	// Doesn't require mapping or unmapping, can be called any number of times,
	auto GetMappedData() const -> void*;

	// Map buffer (Only CPU)
	// Buffer must be unmapped the same number of times as it was mapped
	auto Map() -> void*;

	// Unmap mapped buffer
	void Unmap();

	// Get pointer to owning device
	inline auto GetDevice() const -> Device& { return *GetOwner(); }

	// ResourceBase override
	auto GetResourceTypeName() const -> char const* override;

  private:
	friend Command;
	friend Device;

	void AddUsageFlags(vk::BufferUsageFlags& usage, u64& size);

	// This is needed for staging buffer to be member of Device,
	// Its shared_ptr is not initialized
	VmaAllocation           allocation;
	VmaAllocationInfo       allocation_info;
	vk::DeviceSize          size;
	vk::BufferUsageFlags    usage;
	vk::MemoryPropertyFlags memory;
};

class StagingBuffer : public Buffer {
  public:
	// No-op constructor
	StagingBuffer() = default;

	// RAII constructor, calls Create
	StagingBuffer(Device& device, vk::DeviceSize const size, std::string_view const name = "");

	// Move constructor
	StagingBuffer(StagingBuffer&& other) noexcept;

	// Move assignment
	StagingBuffer& operator=(StagingBuffer&& other) noexcept;

	// Create with result checked
	auto Create(Device& device, vk::DeviceSize const size, std::string_view const name) -> vk::Result;

	void Reset();
	auto GetPtr() const -> u8*;
	auto GetOffset() const -> u32;
	
	// Do not call
	inline auto SetOffset(u32 const offset) -> void { this->offset = offset; }

  private:
	u32 offset = 0;
};

class BindlessBuffer : public Buffer, public BindlessResourceBase {
  public:
	// No-op constructor
	BindlessBuffer() = default;

	// RAII constructor, calls Create
	BindlessBuffer(Device& device, BindlessDescriptor& descriptor, BindlessBufferInfo const& info);

	// Move constructor
	BindlessBuffer(BindlessBuffer&& other) noexcept;

	// Move assignment
	BindlessBuffer& operator=(BindlessBuffer&& other) noexcept;

	// Destructor, frees resources
	~BindlessBuffer();

	// Create with result checked
	auto Create(Device& device, BindlessDescriptor& descriptor, BindlessBufferInfo const& info) -> vk::Result;

	// Frees all resources
	void Free() override;
};

} // namespace VB_NAMESPACE
