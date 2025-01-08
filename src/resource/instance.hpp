#ifndef VULKAN_BACKEND_RESOURCE_INSTANCE_HPP_
#define VULKAN_BACKEND_RESOURCE_INSTANCE_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <set>
#include <string_view>
#else
import std;
#endif

#include <vulkan/vulkan.h>

#include "common.hpp"
#include "vulkan_backend/interface/instance.hpp"
#include "base.hpp"
#include "physical_device.hpp"

namespace VB_NAMESPACE {
struct InstanceResource: NoCopyNoMove, std::enable_shared_from_this<InstanceResource> {
	VkInstance handle = VK_NULL_HANDLE;
	VkAllocationCallbacks* allocator = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkDebugReportCallbackEXT debugReport = VK_NULL_HANDLE;
	char const* applicationName = "Unnamed Application";
	char const* engineName = "Unnamed Engine";

	u32 apiVersion;

	std::vector<bool>                  activeLayers; // Available layers
	std::vector<char const*>           activeLayersNames;
	std::vector<VkLayerProperties>     layers;

	std::vector<bool>                  activeExtensions;     // Instance Extensions
	std::vector<char const*>           activeExtensionsNames; // Instance Extensions
	std::vector<VkExtensionProperties> extensions;   // Instance Extensions

	std::vector<char const*> requiredInstanceExtensions {
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	};

	std::vector<PhysicalDeviceResource> physicalDevices;

	std::set<ResourceBase<InstanceResource>*> resources;

	InstanceInfo init_info;

	void Create(InstanceInfo const& info);
	void GetPhysicalDevices();

	auto CreateDebugUtilsMessenger(VkDebugUtilsMessengerCreateInfoEXT const& info) -> VkResult;
	auto CreateDebugReportCallback(VkDebugReportCallbackCreateInfoEXT const& info) -> VkResult;

	void DestroyDebugUtilsMessenger();
	void DestroyDebugReportCallback();

	auto GetName() const -> char const*;
	auto GetType() const -> char const*;
	auto GetInstance() -> InstanceResource*;
	void Free();

	~InstanceResource() {
		Free();
	}
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_RESOURCE_INSTANCE_HPP_