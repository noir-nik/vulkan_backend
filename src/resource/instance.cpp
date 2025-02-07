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

#include "vulkan_backend/interface/physical_device/info.hpp"
#include "vulkan_backend/interface/instance/instance.hpp"
#include "vulkan_backend/constants/constants.hpp"
#include "vulkan_backend/interface/instance/info.hpp"
#include "vulkan_backend/interface/physical_device/physical_device.hpp"
#include "vulkan_backend/vk_result.hpp"
#include "vulkan_backend/log.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/util/algorithm.hpp"
#include "vulkan_backend/vulkan_functions.hpp"

namespace VB_NAMESPACE {
vk::Bool32 DebugUtilsCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
    vk::DebugUtilsMessageTypeFlagsEXT              messageTypes,
    const vk::DebugUtilsMessengerCallbackDataEXT*  pCallbackData,
    void*                                          pUserData ) {
	if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) {
		VB_LOG_MESSAGE(LogLevel::Trace, pCallbackData->pMessage);
	}
	if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
		VB_LOG_MESSAGE(LogLevel::Info, pCallbackData->pMessage);
	}
	if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
		VB_LOG_MESSAGE(LogLevel::Warning, pCallbackData->pMessage);
	}
	if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
		VB_LOG_MESSAGE(LogLevel::Error, pCallbackData->pMessage);
	}
	return vk::False;
}

VkBool32 DebugUtilsCallbackCompat(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData) {
	return DebugUtilsCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT(messageSeverity),
							  vk::DebugUtilsMessageTypeFlagsEXT(messageTypes),
							  reinterpret_cast<const vk::DebugUtilsMessengerCallbackDataEXT*>(pCallbackData), pUserData);
}

Instance::Instance(InstanceCreateInfo const& info) {
	auto result = Create(info);
	if (result != vk::Result::eSuccess) {
		LogCreationError(result);
	}
	VB_ASSERT(result == vk::Result::eSuccess, "Failed to create instance!");
}

// Instance is not a owned by any other class
Instance::~Instance() {
	Free();
}

void Instance::LogCreationError(vk::Result result) {
	VB_LOG_ERROR("Failed to create instance: %s", vk::to_string(result).c_str());
}

auto Instance::Create(InstanceCreateInfo const& info) -> vk::Result {
	application_info = info.application_info;
	SetAllocator(info.allocator);

	// get api version
	VB_VK_RESULT result = vk::enumerateInstanceVersion(&application_info.apiVersion);
	VB_CHECK_VK_RESULT(result, "Failed to get api version!");

	auto SupportsLayer = [this](std::string_view layer) -> bool {
		return std::any_of(available_layers.begin(), available_layers.end(),
			[&layer](vk::LayerProperties const& availableLayer) {
				return availableLayer.layerName == layer;
			});
	};

	auto SupportsExtension = [this](std::string_view extension) -> bool {
		return algo::SupportsExtension(available_extensions, extension);
	};

	auto SupportsLayers = [SupportsLayer](std::span<char const* const> layers) -> bool {
		return std::all_of(layers.begin(), layers.end(), SupportsLayer);
	};

	std::tie(result, available_layers) = vk::enumerateInstanceLayerProperties();
	VB_CHECK_VK_RESULT(result, "Failed to get available layers!");
	std::tie(result, available_extensions) = vk::enumerateInstanceExtensionProperties();
	VB_CHECK_VK_RESULT(result, "Failed to get available extensions!");

	if (!SupportsLayers(info.required_layers)) {
		return vk::Result::eErrorLayerNotPresent;
	}
	if (!algo::SupportsExtensions(available_extensions, info.required_extensions)) {
		return vk::Result::eErrorExtensionNotPresent;
	}
	enabled_layers.insert(enabled_layers.end(), info.required_layers.begin(), info.required_layers.end());
	enabled_extensions.insert(enabled_extensions.end(), info.required_extensions.begin(), info.required_extensions.end());

	// Add optional layers and extensions
	std::copy_if(info.optional_extensions.begin(), info.optional_extensions.end(),
		std::back_inserter(enabled_extensions), SupportsExtension);
	std::copy_if(info.optional_layers.begin(), info.optional_layers.end(),
		std::back_inserter(enabled_layers), SupportsLayer);
	
	// Check if validation layer is enabled
	if (algo::SpanContainsString(enabled_layers, kValidationLayerName)) {
		validation_enabled = true;
	}

	// Reuqires new headers
	vk::DebugUtilsMessengerCreateInfoEXT constexpr kDebugUtilsCreateInfo = {
		.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
						   // vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
						   vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
		.messageType =
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding,
		.pfnUserCallback = DebugUtilsCallback, // The problem is here
		.pUserData = nullptr,
	};

	VkDebugUtilsMessengerCreateInfoEXT constexpr kDebugUtilsCreateInfoCompat = {
		.sType			 = VkStructureType(kDebugUtilsCreateInfo.sType),
		.flags			 = VkDebugUtilsMessengerCreateFlagsEXT(kDebugUtilsCreateInfo.flags),
		.messageSeverity = VkDebugUtilsMessageSeverityFlagsEXT(kDebugUtilsCreateInfo.messageSeverity),
		.messageType	 = VkDebugUtilsMessageTypeFlagsEXT(kDebugUtilsCreateInfo.messageType),
		.pfnUserCallback = DebugUtilsCallbackCompat,
		.pUserData		 = kDebugUtilsCreateInfo.pUserData,
	};

	bool bDebugUtilsEnabled = algo::SpanContainsString(enabled_extensions, vk::EXTDebugUtilsExtensionName);

	std::tie(result, static_cast<vk::Instance&>(*this)) = vk::createInstance(
		{
			.pNext = bDebugUtilsEnabled ? &kDebugUtilsCreateInfo : nullptr,
			.pApplicationInfo = &info.application_info,
			.enabledLayerCount = static_cast<u32>(enabled_layers.size()),
			.ppEnabledLayerNames = enabled_layers.data(),
			.enabledExtensionCount = static_cast<u32>(enabled_extensions.size()),
			.ppEnabledExtensionNames = enabled_extensions.data(),
		},
		allocator);

	VB_CHECK_VK_RESULT(result, "Failed to create Vulkan instance!");
	VB_LOG_TRACE("Created instance.");

	// Debug Utils
	if (bDebugUtilsEnabled) {
		// Only with dynamic loader
		LoadInstanceDebugUtilsFunctionsEXT(*this);
		std::tie(result, debug_messenger) = createDebugUtilsMessengerEXT((kDebugUtilsCreateInfo), allocator);
		VB_CHECK_VK_RESULT(result, "Failed to set up debug messenger!");
		VB_LOG_TRACE("Created debug messenger.");
	}

	// Layer settings
	bool requiresLayerSettings = algo::SpanContainsString(enabled_layers, vk::EXTLayerSettingsExtensionName);
	// todo: add layer settings extension
	
	// Get physical devices
	CreatePhysicalDevices();
	return vk::Result::eSuccess;
}

void Instance::CreatePhysicalDevices() {
	// VB_VLA(vk::PhysicalDevice, vk_devices, physical_devices.size());
	// std::vector<vk::PhysicalDevice> physical_devices_vk;
	VB_VK_RESULT result;
	std::tie(result, physical_devices) = enumeratePhysicalDevices();
	VB_CHECK_VK_RESULT(result, "Failed to enumerate physical devices!");
	VB_ASSERT(!physical_devices.empty(), "no GPUs with Vulkan support!");
	VB_LOG_TRACE("Found %d physical device(s).", physical_devices.size());
	// physical_devices.reserve(physical_devices_vk.size());
	// for (auto& device_vk : physical_devices_vk) {
	// 	physical_devices.emplace_back(device_vk);
	// 	physical_devices.back().GetDetails();
	// };
}

auto Instance::GetName() const -> char const* {
	return application_info.pApplicationName;
}

auto Instance::GetResourceTypeName() const -> char const* {
	return "InstanceResource";
}

auto Instance::SetAllocator(vk::AllocationCallbacks* allocator) -> void {
	this->allocator = allocator;
	if (allocator) {
		allocator_object = *allocator;
	}
}

void Instance::Free() {
	VB_LOG_TRACE("[ Free ] Destroying instance...");
	physical_devices.clear();
	available_extensions.clear();
	available_layers.clear();
	enabled_layers.clear();
	enabled_extensions.clear();

	if (debug_messenger) {
		destroyDebugUtilsMessengerEXT(debug_messenger, allocator);
		VB_LOG_TRACE("[ Free ] Destroyed debug messenger.");
		debug_messenger = nullptr;
	}

	destroy(allocator);
	VB_LOG_TRACE("[ Free ] Destroyed instance.");
}
} // namespace VB_NAMESPACE
