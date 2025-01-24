#ifndef VB_USE_STD_MODULE
#include <string_view>
#include <algorithm>
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/util/algorithm.hpp"

namespace VB_NAMESPACE {
namespace algo {

} // namespace algo
} // namespace VB_NAMESPACE