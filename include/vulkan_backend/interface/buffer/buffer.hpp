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

#ifndef VB_DEV
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif

#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/types.hpp"
#include "vulkan_backend/classes/base.hpp"
#include "vulkan_backend/classes/gpu_resource.hpp"
#include "vulkan_backend/interface/buffer/info.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
class Buffer : public vk::Buffer, public Named, public GpuResource, public ResourceBase<Device> {
public:
	Buffer(Device& device, BufferInfo const& info);
	Buffer(Buffer&& other) noexcept;
	Buffer& operator=(Buffer&& other) noexcept;
	~Buffer();
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
	inline auto GetDevice() const -> Device& { return *owner; }
	
	// ResourceBase override
	auto GetResourceTypeName() const -> char const* override;

	inline auto GetBinding() const -> u32 { return info.binding; }
	inline void SetBinding(u32 binding) { info.binding = binding; }
private:
	Buffer() = default;
	friend Command;
	friend Device;
	void Create(Device& device, BufferInfo const& info);
	void Free() override;
	void AddUsageFlags(vk::BufferUsageFlags & usage, u64& size);

	// This is needed for staging buffer to be member of Device,
	// Its shared_ptr is not initialized
	VmaAllocation           allocation;
	VmaAllocationInfo       allocation_info;
	BufferInfo              info;
};

class StagingBuffer : public Buffer {
public:
	StagingBuffer(Device& device, u32 size, std::string_view name = "Staging Buffer");

	void Reset();
	auto GetPtr() const -> u8*;
	auto GetOffset() const -> u32;

	u32 offset = 0;
};

} // namespace VB_NAMESPACE

