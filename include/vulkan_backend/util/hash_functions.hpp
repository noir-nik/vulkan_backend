#pragma once

#ifndef VB_USE_STD_MODULE
#include <cstddef>
#include <span>
#include <string_view>
#elif defined(VB_DEV)
import std;
#endif

#include "vulkan_backend/config.hpp"
#include "vulkan_backend/types.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
u64 inline HashFnv1a64(char const* data, std::size_t size) {
	u64 hash = 0xcbf29ce484222325UL;
	for (std::size_t i = 0; i < size; ++i) {
		hash ^= data[i];
		hash *= 0x00000100000001B3UL;
	}
	return hash;
}

u64 inline HashFnv1a64(std::string_view data) {
	return HashFnv1a64(data.data(), data.size());
}

} // namespace VB_NAMESPACE