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

VB_EXPORT
namespace VB_NAMESPACE {	
enum class LogLevel {
	Trace,
	Info,
	Warning,
	Error,
	None,
};

void CheckVkResultDefault(int result, char const* message);
void MessageCallbackDefault(LogLevel level, char const* message);
auto StringFromVkResult(int result) -> char const*;

struct InstanceInfo {
	// Function pointer to check VkResult values
	void (&checkVkResult)(int, char const*) = CheckVkResultDefault;
	void (&MessageCallback)(LogLevel, char const*) = MessageCallbackDefault;
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
	void SetLogLevel(LogLevel level);

private:
	std::shared_ptr<InstanceResource> resource;
	friend auto CreateInstance(InstanceInfo const& info) -> Instance;
};

auto CreateInstance(InstanceInfo const& info = {}) -> Instance;
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_INSTANCE_HPP_