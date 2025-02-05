#pragma once

#ifndef VB_USE_STD_MODULE
#include <span>
#include <unordered_set>
#elif defined(VB_DEV)
import std;
#endif

#include "vulkan_backend/classes/no_copy_no_move.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/instance/info.hpp"
#include "vulkan_backend/interface/physical_device/info.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
class Instance : public vk::Instance, NoCopyNoMove {
  public:
	// static factory, returns {} on fail
	Instance(InstanceCreateInfo const& info);
	~Instance();

	// Getters
	auto GetName() const -> char const*;
	auto GetResourceTypeName() const -> char const*;
	// auto GetPhysicalDevices()  -> std::span<PhysicalDevice>;
	auto		GetAllocator() const -> vk::AllocationCallbacks const* { return allocator; }
	void		SetAllocator(vk::AllocationCallbacks* allocator);
	inline auto GetPhysicalDevices() const -> std::span<vk::PhysicalDevice const> { return physical_devices; }
	inline auto IsValidationEnabled() const -> bool { return validation_enabled; }
	inline auto IsDebugUtilsEnabled() const -> bool {
		return debug_messenger != vk::DebugUtilsMessengerEXT{};
	}

	std::vector<vk::PhysicalDevice>		 physical_devices;
	std::vector<vk::ExtensionProperties> available_extensions;
	std::vector<vk::LayerProperties>	 available_layers;
	std::vector<char const*>			 enabled_layers;
	std::vector<char const*>			 enabled_extensions;
	vk::DebugUtilsMessengerEXT			 debug_messenger = nullptr;
	vk::AllocationCallbacks*			 allocator		 = nullptr;
	vk::ApplicationInfo					 application_info;
	bool								 validation_enabled = false;

  private:
	auto					Create(InstanceCreateInfo const& info) -> vk::Result;
	void					LogCreationError(vk::Result result);
	void					CreatePhysicalDevices();
	void					Free();
	vk::AllocationCallbacks allocator_object = {};
};
} // namespace VB_NAMESPACE
