#pragma once

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"

#define VB_VK_RESULT [[maybe_unused]] vk::Result

#ifdef VB_DISABLE_VK_RESULT_CHECK
#define VB_CHECK_VK_RESULT(result, message) (void(0))
#else
#define VB_CHECK_VK_RESULT(result, message) \
	CheckVkResult(result, message);
#endif

VB_EXPORT
namespace VB_NAMESPACE {
// Default function to check result. Print message and abort on error.
void CheckVkResultDefault(vk::Result result, char const* message);

// Helper function to convert result to string
auto StringFromVkResult(vk::Result result) -> char const*;

// Set function to check all vk::Result values
void SetCheckVkResultFunction(void (&func)(vk::Result, char const*));

extern void (&CheckVkResult)(vk::Result, char const*);
} // namespace VB_NAMESPACE
