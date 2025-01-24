#pragma once

#ifndef VB_USE_STD_MODULE
#include <memory>
#include <span>
#include <unordered_set>
#elif defined(VB_DEV)
import std;
#endif

#include "vulkan_backend/classes/no_copy_no_move.hpp"
#include "vulkan_backend/interface/info/physical_device.hpp"
#include "vulkan_backend/interface/info/instance.hpp"
#include "vulkan_backend/classes/base.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/info/device.hpp"
// #include "vulkan_backend/interface/physical_device.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
class Instance : public vk::Instance, public OwnerBase<Instance> {
public:
	// static factory, returns {} on fail
	static auto Create(InstanceCreateInfo const& info) -> std::shared_ptr<Instance>;
	~Instance();
	
	// Device creation factory function
	[[nodiscard]] auto CreateDevice(PhysicalDevice* physical_device, DeviceInfo const& info)
		-> std::shared_ptr<Device>;

	// Selects a physical device, checks if it supports the required extensions and queues.
	// returns: selected physical device that meet the requirements or nullptr if none was selected
	auto SelectPhysicalDevice(PhysicalDeviceSelectInfo const& info = {}) -> PhysicalDevice*;

	// Getters
	auto GetName() const -> char const*;
	auto GetResourceTypeName() const -> char const*;
	// auto GetPhysicalDevices()  -> std::span<PhysicalDevice>;
	auto GetAllocator() const -> vk::AllocationCallbacks const* { return allocator; }
	inline auto IsValidationEnabled() const -> bool { return validation_enabled; }
	inline auto IsDebugUtilsEnabled() const -> bool { return debug_messenger != vk::DebugUtilsMessengerEXT{}; }

	std::vector<PhysicalDevice>                  physical_devices;
	std::vector<vk::ExtensionProperties>         available_extensions;
	std::vector<vk::LayerProperties>             available_layers;
	std::vector<char const*>                     enabled_layers;
	std::vector<char const*>                     enabled_extensions;
	vk::DebugUtilsMessengerEXT                   debug_messenger = nullptr;
	vk::AllocationCallbacks*                     allocator = nullptr;
	vk::ApplicationInfo                          application_info;
	bool										 validation_enabled = false;
  private:
	vk::AllocationCallbacks allocator_object = {};
	Instance() = default;
	auto CreateImpl(InstanceCreateInfo const& info) -> vk::Result;
	void LogCreationError(vk::Result result);
	void CreatePhysicalDevices();
	void Free();
};
} // namespace VB_NAMESPACE

