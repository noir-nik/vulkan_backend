#ifndef VULKAN_BACKEND_UTIL_HPP_
#define VULKAN_BACKEND_UTIL_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <cstddef>
#else
import std;
#endif

#include "vulkan_backend/config.hpp"

namespace VB_NAMESPACE {
static inline auto HashCombine(std::size_t hash, std::size_t x) -> std::size_t {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return hash ^ x + 0x9e3779b9 + (hash << 6) + (hash >> 2);
}
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_UTIL_HPP_
