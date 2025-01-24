#pragma once

#include "vulkan_backend/config.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
template <typename T, typename U>
inline auto TestBits(T const value, U const bits) -> bool {
	return (value & bits) == bits;
}

} // namespace VB_NAMESPACE
