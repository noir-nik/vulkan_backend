#pragma once

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
void LoadInstanceDebugUtilsFunctionsEXT(vk::Instance instance);
void LoadDeviceDebugUtilsFunctionsEXT(vk::Device device);
void LoadInstanceCooperativeMatrixFunctionsKHR(vk::Instance instance);
void LoadInstanceCooperativeMatrix2FunctionsNV(vk::Instance instance);
} // namespace VB_NAMESPACE
