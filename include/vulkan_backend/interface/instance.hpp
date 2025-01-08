#ifndef VULKAN_BACKEND_INSTANCE_HPP_
#define VULKAN_BACKEND_INSTANCE_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <memory>
#include <span>
#elif defined(VB_DEV)
import std;
#endif

#include "../fwd.hpp"
#include "device.hpp"
#include "../io.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
struct InstanceInfo {
	// Function pointer to check VkResult values
	void (&checkVkResult)(vk::Result, char const*) = CheckVkResultDefault;
	void (&message_callback)(LogLevel, char const*) = MessageCallbackDefault;
	// Validation
	bool validation_layers      = false;
	bool debug_report           = false;

	// Additional instance extensions
	std::span<char const*> extensions = {};
};

class Instance {
public:
	[[nodiscard]] auto CreateDevice(DeviceInfo const& info = {}) -> Device;
	auto GetHandle() const -> vk::Instance;
	void SetMessageCallback(void (&callback)(LogLevel, char const*));

private:
	std::shared_ptr<InstanceResource> resource;
	friend auto CreateInstance(InstanceInfo const& info) -> Instance;
};

auto CreateInstance(InstanceInfo const& info = {}) -> Instance;
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_INSTANCE_HPP_