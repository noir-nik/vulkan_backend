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

#include "vulkan_backend/types.hpp"
#include "vulkan_backend/classes/structs.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
struct BufferInfo {
	u32 static constexpr kBindingNone = ~0u;
	
	u64                     size;
	vk::BufferUsageFlags    usage;
	MemoryFlags             memory = Memory::GPU;
	std::string_view        name = "";
	// binding to write bindless descriptor to (Only GPU)
	u32                     binding = kBindingNone;
};

} // namespace VB_NAMESPACE
