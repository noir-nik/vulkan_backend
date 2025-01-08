#ifndef VULKAN_BACKEND_RESOURCE_COMMON_HPP_
#define VULKAN_BACKEND_RESOURCE_COMMON_HPP_

#include "vulkan_backend/config.hpp"

namespace VB_NAMESPACE {
struct NoCopy {
	NoCopy() = default;
	NoCopy(const NoCopy&) = delete;
	NoCopy& operator=(const NoCopy&) = delete;
	NoCopy(NoCopy&&) = default;
	NoCopy& operator=(NoCopy&&) = default;
	~NoCopy() = default;
};

struct NoCopyNoMove {
	NoCopyNoMove() = default;
	NoCopyNoMove(const NoCopyNoMove&) = delete;
	NoCopyNoMove& operator=(const NoCopyNoMove&) = delete;
	NoCopyNoMove(NoCopyNoMove&&) = delete;
	NoCopyNoMove& operator=(NoCopyNoMove&&) = delete;
	~NoCopyNoMove() = default;
};
} // namespace VB_NAMESPACE
#endif // VULKAN_BACKEND_RESOURCE_COMMON_HPP_