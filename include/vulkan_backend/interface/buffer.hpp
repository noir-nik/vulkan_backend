#ifndef VULKAN_BACKEND_BUFFER_HPP_
#define VULKAN_BACKEND_BUFFER_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <string_view>
#include <memory>
#elif defined(VB_DEV)
import std;
#endif

#include "../structs.hpp"
#include "../fwd.hpp"

VB_EXPORT
namespace VB_NAMESPACE {	
struct BufferInfo {
	u64                     size;
	BufferUsageFlags        usage;
	MemoryFlags             memory = Memory::GPU;
	std::string_view        name = "";
};

class Buffer {
	std::shared_ptr<BufferResource> resource;
	friend Device;
	friend Command;
	friend DeviceResource;
public:
	auto GetResourceID() const -> u32;
	auto GetSize() const -> u64;
	auto GetMappedData() -> void*;
	auto GetAddress() -> DeviceAddress;
	auto Map() -> void*;
	void Unmap();
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_BUFFER_HPP_