#pragma once

#pragma once

#ifndef VB_USE_STD_MODULE
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

#include "vulkan_backend/config.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
VmaAllocationCreateFlags constexpr inline kBufferCpuFlags = {
	VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT};
} // namespace VB_NAMESPACE

