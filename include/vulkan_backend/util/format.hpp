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

#include "vulkan_backend/config.hpp"

namespace VB_NAMESPACE {
namespace detail {

auto constexpr inline FormatName(std::string_view name) -> std::string_view {
	return name.size() > 0 ? name : "\"\"";
}

auto constexpr inline Format(vk::MemoryPropertyFlags memory) -> std::string_view {
	return memory & vk::MemoryPropertyFlagBits::eDeviceLocal ? "GPU" : "CPU";
}

} // namespace util
} // namespace VB_NAMESPACE