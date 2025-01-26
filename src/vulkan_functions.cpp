#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include <vulkan/vulkan.h>

#include "vulkan_backend/vulkan_functions.hpp"

#ifdef VK_EXT_debug_utils
PFN_vkCreateDebugUtilsMessengerEXT	pfnVkCreateDebugUtilsMessengerEXT = nullptr;
PFN_vkDestroyDebugUtilsMessengerEXT pfnVkDestroyDebugUtilsMessengerEXT = nullptr;
PFN_vkSetDebugUtilsObjectNameEXT	pfnVkSetDebugUtilsObjectNameEXT = nullptr;

#ifndef VB_NO_LOAD_FUNCTIONS
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
	VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger) {
	return pfnVkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(VkInstance					instance,
														   VkDebugUtilsMessengerEXT		messenger,
														   VkAllocationCallbacks const* pAllocator) {
	return pfnVkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL vkSetDebugUtilsObjectNameEXT(VkDevice							 device,
															const VkDebugUtilsObjectNameInfoEXT* pNameInfo) {
	return pfnVkSetDebugUtilsObjectNameEXT(device, pNameInfo);
}
#endif // !VB_NO_LOAD_FUNCTIONS

namespace VB_NAMESPACE {
void LoadInstanceDebugUtilsFunctionsEXT(vk::Instance instance) {
	pfnVkCreateDebugUtilsMessengerEXT =
		(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	pfnVkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		instance, "vkDestroyDebugUtilsMessengerEXT");
}

void LoadDeviceDebugUtilsFunctionsEXT(vk::Device device) {
	pfnVkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
		vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectName"));
}
} // namespace VB_NAMESPACE
#else  // VK_EXT_debug_utils
namespace VB_NAMESPACE {
void LoadInstanceDebugUtilsFunctionsEXT(vk::Instance instance) {}
void LoadDeviceDebugUtilsFunctionsEXT(vk::Device device) {}
} // namespace VB_NAMESPACE
#endif // VK_EXT_debug_utils

#ifdef VK_KHR_cooperative_matrix
PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR = nullptr;

#ifndef VK_NO_LOAD_FUNCTIONS
VKAPI_ATTR VkResult VKAPI_CALL
vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount,
												  VkCooperativeMatrixPropertiesKHR* pProperties) {
													// auto t = pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR;
	return pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(physicalDevice, pPropertyCount, pProperties);
}
#endif // !VK_NO_LOAD_FUNCTIONS
namespace VB_NAMESPACE {
void LoadInstanceCooperativeMatrixFunctionsKHR(vk::Instance instance) {
	pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR =
		(PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR)(vkGetInstanceProcAddr(
			instance, "vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR"));
}
} // namespace VB_NAMESPACE
#else  // VK_KHR_cooperative_matrix
namespace VB_NAMESPACE {
void LoadDeviceCooperativeMatrixFunctionsKHR(vk::Device device) {}
} // namespace VB_NAMESPACE
#endif // VK_KHR_cooperative_matrix
/* 
#ifdef VK_NV_cooperative_matrix
PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV;
#ifndef VK_NO_LOAD_FUNCTIONS
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceCooperativeMatrixPropertiesNV(
	VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkCooperativeMatrixPropertiesNV* pProperties) {
	return pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV(physicalDevice, pPropertyCount, pProperties);
}
#endif // !VK_NO_LOAD_FUNCTIONS

namespace VB_NAMESPACE {
void LoadDeviceCooperativeMatrixFunctionsNV(vk::Device device) {
	pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV =
		(PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV)(vkGetDeviceProcAddr(
			device, "vkGetPhysicalDeviceCooperativeMatrixPropertiesNV"));
}
} // namespace VB_NAMESPACE
#else  // VK_NV_cooperative_matrix
namespace VB_NAMESPACE {
void LoadDeviceCooperativeMatrixFunctionsNV(vk::Device device) {}
} // namespace VB_NAMESPACE
#endif // VK_NV_cooperative_matrix
 */
#ifdef VK_NV_cooperative_matrix2
PFN_vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV
	pfn_vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV = nullptr;

#ifndef VK_NO_LOAD_FUNCTIONS
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV(
	VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount,
	VkCooperativeMatrixFlexibleDimensionsPropertiesNV* pProperties) {
	return pfn_vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV(
		physicalDevice, pPropertyCount, pProperties);
}
#endif // !VK_NO_LOAD_FUNCTIONS

namespace VB_NAMESPACE {
void LoadInstanceCooperativeMatrix2FunctionsNV(vk::Instance instance) {
	pfn_vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV =
		(PFN_vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV)(vkGetInstanceProcAddr(
			instance, "vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV"));
}
} // namespace VB_NAMESPACE
#else  // VK_NV_cooperative_matrix2
namespace VB_NAMESPACE {
void LoadDeviceCooperativeMatrix2FunctionsNV(vk::Device device) {}
} // namespace VB_NAMESPACE
#endif // VK_NV_cooperative_matrix2