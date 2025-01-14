#ifndef VB_USE_STD_MODULE
#include <set>
#include <string_view>
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include <vulkan/vulkan.h>

#include "instance.hpp"
#include "device.hpp"

namespace VB_NAMESPACE {
auto Instance::CreateDevice(DeviceInfo const& info) -> Device {
	Device device;
	device.resource = MakeResource<DeviceResource>(resource.get(), "device ");
	device.resource->Create(info);
	return device;
}

auto Instance::GetHandle() const -> vk::Instance {
	return resource->handle;
}

namespace {
static VKAPI_ATTR VkBool32 VKAPI_CALL
DebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
				   VkDebugUtilsMessageTypeFlagsEXT messageType,
				   const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
				   void *pUserData) {
	auto GetInstance = [pUserData]() -> InstanceResource * {
		return reinterpret_cast<InstanceResource *>(pUserData);
	};

	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
		VB_LOG_TRACE("[ Validation Layer ] %s", pCallbackData->pMessage);
	}
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		VB_LOG_TRACE("[ Validation Layer ] %s", pCallbackData->pMessage);
	}
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		VB_LOG_WARN("[ Validation Layer ] %s", pCallbackData->pMessage);
	}
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		VB_LOG_ERROR("[ Validation Layer ] %s", pCallbackData->pMessage);
	}

	return VK_FALSE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
DebugReportCallback(VkDebugReportFlagsEXT flags,
					VkDebugReportObjectTypeEXT objectType,
					uint64_t object,
					size_t location,
					i32 messageCode,
					char const* pLayerPrefix,
					char const* pMessage,
					void* pUserData) {
	auto GetInstance = [pUserData]() -> InstanceResource * {
		return reinterpret_cast<InstanceResource *>(pUserData);
	};
	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)        
		{ VB_LOG_TRACE ("[Debug Report] %s", pMessage); }
	if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)            
		{ VB_LOG_WARN ("[Debug Report] %s", pMessage); }
	if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		{ VB_LOG_WARN ("[Debug Report Performance] %s", pMessage); }
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)              
		{ VB_LOG_ERROR("[Debug Report] %s", pMessage); }
	if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)              
		{ VB_LOG_TRACE("[Debug Report] %s", pMessage); }

	return VK_FALSE;
}

void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
	createInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
						   // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
						   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
		.pfnUserCallback = DebugUtilsCallback,
		.pUserData = nullptr,
	};
}

void PopulateDebugReportCreateInfo(VkDebugReportCallbackCreateInfoEXT& createInfo) {
	createInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
		.flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
				 VK_DEBUG_REPORT_WARNING_BIT_EXT |
				 VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
				 VK_DEBUG_REPORT_ERROR_BIT_EXT |
				 VK_DEBUG_REPORT_DEBUG_BIT_EXT,
		.pfnCallback = DebugReportCallback,
		.pUserData = nullptr
	};
}
} // debug report

void InstanceResource::Create(InstanceInfo const& info){
	init_info.~InstanceInfo();
	new(&init_info) InstanceInfo(info);

	// optional data, provides useful info to the driver
	VkApplicationInfo appInfo{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = applicationName,
		.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
		.pEngineName = engineName,
		.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
		.apiVersion = VK_API_VERSION_1_3,
	};

	// get api version
	vkEnumerateInstanceVersion(&apiVersion);

	// Validation layers
	if (info.validation_layers) {
		// get all available layers
		u32 layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		layers.resize(layerCount);
		activeLayers.resize(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

		// active default khronos validation layer
		std::string_view constexpr validation_layer_name("VK_LAYER_KHRONOS_validation");
		bool khronosAvailable = false;
		for (size_t i = 0; i < layerCount; i++) {
			activeLayers[i] = false;
			if (validation_layer_name == layers[i].layerName) {
				activeLayers[i] = true;
				khronosAvailable = true;
				break;
			}
		}
		if (!khronosAvailable) {VB_LOG_WARN("Default validation layer not available!");}

		for (size_t i = 0; i < layerCount; i++) {
			if (activeLayers[i]) {
				activeLayersNames.push_back(layers[i].layerName);
			}
		}
	}

	requiredInstanceExtensions.insert(requiredInstanceExtensions.end(),
		init_info.extensions.begin(), init_info.extensions.end());
	
	// Extensions
	if (info.validation_layers) {
		requiredInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		if (info.debug_report) {
			requiredInstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
	}

	// get all available extensions
	u32 extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, 0);
	extensions.resize(extensionCount);
	activeExtensions.resize(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	// set to active all extensions that we enabled //TODO(nm): Rewrite loop
	for (size_t i = 0; i < requiredInstanceExtensions.size(); i++) {
		for (size_t j = 0; j < extensionCount; j++) {
			if (std::strcmp(requiredInstanceExtensions[i], extensions[j].extensionName) == 0) {
				activeExtensions[j] = true;
				break;
			}
		}
	}

	// Enable validation features if available
	bool validation_features = false;
	if (info.validation_layers){
		for (size_t i = 0; i < extensionCount; i++) {
			if (std::strcmp(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME, extensions[i].extensionName) == 0) {
				validation_features = true;
				activeExtensions[i] = true;
				VB_LOG_TRACE("%s is available, enabling it", VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
				break;
			}
		}
	}

	VkInstanceCreateInfo createInfo{
		.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo        = &appInfo,
		.enabledExtensionCount   = static_cast<u32>(activeExtensionsNames.size()),
		.ppEnabledExtensionNames = activeExtensionsNames.data(),
	};

	VkValidationFeaturesEXT validationFeaturesInfo;
	VkValidationFeatureEnableEXT enableFeatures[] = {
		VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
		VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
		VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
	};

	if (validation_features) {
		validationFeaturesInfo = {
			.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
			.pNext                         = createInfo.pNext,
			.enabledValidationFeatureCount = std::size(enableFeatures),
			.pEnabledValidationFeatures    = enableFeatures,
		};
		createInfo.pNext = &validationFeaturesInfo;
	}

	// get the name of all extensions that we enabled
	activeExtensionsNames.clear();
	for (size_t i = 0; i < extensionCount; i++) {
		if (activeExtensions[i]) {
			activeExtensionsNames.push_back(extensions[i].extensionName);
		}
	}

	createInfo.enabledExtensionCount = static_cast<u32>(activeExtensionsNames.size());
	createInfo.ppEnabledExtensionNames = activeExtensionsNames.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if (info.validation_layers) {
		createInfo.enabledLayerCount = activeLayersNames.size();
		createInfo.ppEnabledLayerNames = activeLayersNames.data();

		// we need to set up a separate logger just for the instance creation/destruction
		// because our "default" logger is created after
		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		debugCreateInfo.pUserData = this;
		debugCreateInfo.pNext = createInfo.pNext;
		createInfo.pNext      = &debugCreateInfo;
	}

	// Instance creation
	VB_VK_RESULT result = vkCreateInstance(&createInfo, allocator, &handle);
	VB_CHECK_VK_RESULT(init_info.checkVkResult, result, "Failed to create Vulkan instance!");
	VB_LOG_TRACE("Created instance.");
	// Debug Utils
	if (info.validation_layers) {
		VkDebugUtilsMessengerCreateInfoEXT messengerInfo;
		PopulateDebugMessengerCreateInfo(messengerInfo);
		debugCreateInfo.pUserData = this;
		result = CreateDebugUtilsMessenger(messengerInfo);
		VB_CHECK_VK_RESULT(init_info.checkVkResult, result, "Failed to set up debug messenger!");
		VB_LOG_TRACE("Created debug messenger.");
	}

	// Debug Report
	if (info.validation_layers && info.debug_report) {
		VkDebugReportCallbackCreateInfoEXT debugReportInfo;
		PopulateDebugReportCreateInfo(debugReportInfo);
		debugReportInfo.pUserData = this;
		// Create the callback handle
		result = CreateDebugReportCallback(debugReportInfo);
		VB_CHECK_VK_RESULT(init_info.checkVkResult, result, "Failed to set up debug report callback!");
		VB_LOG_TRACE("Created debug report callback.");
	}

	VB_LOG_TRACE("Created Vulkan Instance.");
}

void InstanceResource::GetPhysicalDevices() {
	// get all devices with Vulkan support
	u32 count = 0;
	vkEnumeratePhysicalDevices(handle, &count, nullptr);
	VB_ASSERT(count != 0, "no GPUs with Vulkan support!");
	VB_VLA(VkPhysicalDevice, physical_devices, count);
	vkEnumeratePhysicalDevices(handle, &count, physical_devices.data());
	VB_LOG_TRACE("Found %d physical device(s).", count);
	physicalDevices.reserve(count);
	for (auto const& device: physical_devices) {
		// device->physicalDevices->handle.insert({UIDGenerator::Next(), PhysicalDevice()});
		physicalDevices.emplace_back(this, device);
		auto& physicalDevice = physicalDevices.back();
		physicalDevice.GetDetails();
	};
}

auto InstanceResource::CreateDebugUtilsMessenger(
		VkDebugUtilsMessengerCreateInfoEXT const& info ) -> VkResult {
	// search for the requested function and return null if cannot find
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr (
		handle,
		"vkCreateDebugUtilsMessengerEXT"
	);
	if (func != nullptr) {
		return func(handle, &info, allocator, &debugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void InstanceResource::DestroyDebugUtilsMessenger() {
	// search for the requested function and return null if cannot find
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr (
		handle,
		"vkDestroyDebugUtilsMessengerEXT"
	);
	if (func != nullptr) {
		func(handle, debugMessenger, allocator);
	}
}

auto InstanceResource::CreateDebugReportCallback(
	VkDebugReportCallbackCreateInfoEXT const& info ) -> VkResult {
	// search for the requested function and return null if cannot find
	auto func = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr (
		handle,
		"vkCreateDebugReportCallbackEXT"
	);
	if (func != nullptr) {
		return func(handle, &info, allocator, &debugReport);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void InstanceResource::DestroyDebugReportCallback() {
	// search for the requested function and return null if cannot find
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr (
		handle,
		"vkDestroyDebugReportCallbackEXT"
	);
	if (func != nullptr) {
		func(handle, debugReport, allocator);
	}
}

auto InstanceResource::GetName() const -> char const* {
	return applicationName;
}

auto InstanceResource::GetType() const -> char const* {
	return "InstanceResource";
}

auto InstanceResource::GetInstance() -> InstanceResource* {
	return this;
}

void InstanceResource::Free() {
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
} // namespace VB_NAMESPACE
