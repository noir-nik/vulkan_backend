#pragma once

#ifndef VB_USE_STD_MODULE
#include <string_view>
#include <memory>
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
#include "vulkan_backend/classes/structs.hpp"
#include "vulkan_backend/classes/gpu_resource.hpp"
#include "vulkan_backend/interface/info/buffer.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
// Buffer handle
using BufferRef = std::shared_ptr<Buffer>;

class Buffer : public vk::Buffer, public Named, public GpuResource, public ResourceBase<Device> {
public:
	Buffer(std::shared_ptr<Device> const& device, BufferInfo const& info);
	~Buffer();
	// Get actual size after alignment
	auto GetSize() const -> u64;

	// Get Device address
	auto GetAddress() const -> vk::DeviceAddress;

	// Get mapped data pointer (Only CPU)
	// Doesn't require mapping or unmapping, can be called any number of times, 
	auto GetMappedData()-> void*;

	// Map buffer (Only CPU)
	// Buffer must be unmapped the same number of times as it was mapped
	auto Map() -> void*;

	// Unmap mapped buffer
	void Unmap();

	// Get pointer to owning device
	inline auto GetDevice() -> Device* { return device; }
	
	// ResourceBase override
	auto GetResourceTypeName() const -> char const* override;
private:
	Buffer() = default;
	friend Command;
	friend Device;
	void Create(Device* device, BufferInfo const& info);
	void Free() override;

	// This is needed for staging buffer to be member of Device,
	// Its shared_ptr is not initialized
	Device*                 device = nullptr;
	VmaAllocation           allocation;
	VmaAllocationInfo       allocationInfo;
	BufferInfo              info;
};
} // namespace VB_NAMESPACE

