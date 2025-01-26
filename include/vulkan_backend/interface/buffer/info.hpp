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

#include "vulkan_backend/classes/structs.hpp"
#include "vulkan_backend/interface/descriptor/descriptor.hpp"
#include "vulkan_backend/types.hpp"


VB_EXPORT
namespace VB_NAMESPACE {
struct BufferInfo {
	u64						size;
	vk::BufferUsageFlags	usage;
	vk::MemoryPropertyFlags memory = Memory::eGPU;
	std::string_view		name   = "";
	// binding to write bindless descriptor to (Only GPU)
	BindlessDescriptor* descriptor = nullptr;
	u32					binding;
};

} // namespace VB_NAMESPACE
