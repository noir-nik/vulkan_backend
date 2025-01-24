#pragma once

#ifndef VB_USE_STD_MODULE
#include <cstdint>
#include <algorithm>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"
#include "vulkan_backend/types.hpp"
#include "vulkan_backend/interface/info/pipeline.hpp"

VB_EXPORT
namespace VB_NAMESPACE {


} // VB_NAMESPACE
