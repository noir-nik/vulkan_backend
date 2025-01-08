#ifndef VULKAN_BACKEND_RESOURCE_INSTANCE_HPP_
#define VULKAN_BACKEND_RESOURCE_INSTANCE_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <set>
#include <string_view>
#else
import std;
#endif

#include <vulkan/vulkan.h>

#ifndef VB_DEV
#if !defined(VB_VMA_IMPLEMENTATION) || VB_VMA_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#endif
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif //VB_DEV

#include <vulkan_backend/interface/device.hpp>
#include <vulkan_backend/interface/instance.hpp>

#include "common.hpp"
#include "base.hpp"
#include "physical_device.hpp"
#include "../util.hpp"

namespace VB_NAMESPACE {
struct InstanceResource: NoCopyNoMove, std::enable_shared_from_this<InstanceResource> {
	VkInstance handle = VK_NULL_HANDLE;
	VkAllocationCallbacks* allocator = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkDebugReportCallbackEXT debugReport = VK_NULL_HANDLE;
	char const* applicationName = "Vulkan";
	char const* engineName = "Engine";

	u32 apiVersion;

	std::vector<bool>              activeLayers; // Available layers
	std::vector<char const*>       activeLayersNames;
	std::vector<VkLayerProperties> layers;

	std::vector<bool>                  activeExtensions;     // Instance Extensions
	std::vector<char const*>           activeExtensionsNames; // Instance Extensions
	std::vector<VkExtensionProperties> extensions;   // Instance Extensions

	std::vector<char const*> requiredInstanceExtensions {
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	};

	std::vector<PhysicalDeviceResource> physicalDevices;

	std::set<ResourceBase<InstanceResource>*> resources;

	InstanceInfo init_info;
	LogLevel log_level;
	void (&MessageCallback)(LogLevel level, char const* message);

	void Create(InstanceInfo const& info);
	void GetPhysicalDevices();

	auto CreateDebugUtilsMessenger(VkDebugUtilsMessengerCreateInfoEXT const& info) -> VkResult;
	auto CreateDebugReportCallback(VkDebugReportCallbackCreateInfoEXT const& info) -> VkResult;

	void DestroyDebugUtilsMessenger();
	void DestroyDebugReportCallback();

	inline auto GetInstance() -> InstanceResource* { return this; }

	auto GetName() -> char const* {
		return "instance";
	}

	auto GetType() -> char const* { 
		return "InstanceResource";
	}

	void Free() {
		activeExtensionsNames.clear();
		activeLayersNames.clear();
		physicalDevices.clear();
		VB_LOG_TRACE("[ Free ] Destroying instance...");
		VB_LOG_TRACE("[ Free ] Cleaning up %zu instance resources...", resources.size());

		std::erase_if(resources, [](ResourceBase<InstanceResource>* const res) {
			res->Free();
			return true;
		});

		if (debugMessenger) {
			DestroyDebugUtilsMessenger();
			VB_LOG_TRACE("[ Free ] Destroyed debug messenger.");
			debugMessenger = nullptr;
		}
		if (debugReport) {
			DestroyDebugReportCallback();
			VB_LOG_TRACE("[ Free ] Destroyed debug report callback.");
			debugReport = nullptr;
		}

		vkDestroyInstance(handle, allocator);
		VB_LOG_TRACE("[ Free ] Destroyed instance.");
	}

	~InstanceResource() {
		Free();
	}
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_RESOURCE_INSTANCE_HPP_