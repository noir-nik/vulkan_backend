#pragma once

#ifndef VB_USE_STD_MODULE
#include <cstdint>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
using i32 = std::int32_t;
using u8  = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using vk::operator&;
using vk::operator|;
using vk::operator^;
using vk::operator~;

} // namespace VB_NAMESPACE
