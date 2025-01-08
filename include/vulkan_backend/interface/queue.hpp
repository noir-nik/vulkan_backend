#ifndef VULKAN_BACKEND_QUEUE_HPP_
#define VULKAN_BACKEND_QUEUE_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <span>
#elif defined(VB_DEV)
import std;
#endif

#include "../fwd.hpp"

VB_EXPORT
namespace VB_NAMESPACE {	
struct QueueInfo {
	QueueFlags flags             = QueueFlagBits::eGraphics;
	bool       separate_family   = false;    // Prefer separate family
	Surface    supported_surface = nullptr;
};

struct SubmitInfo {
	std::span<SemaphoreSubmitInfo const> waitSemaphoreInfos;
	std::span<SemaphoreSubmitInfo const> signalSemaphoreInfos;
};

class Queue {
	QueueResource* resource = nullptr;
	friend SwapChainResource;
	friend Swapchain;
	friend CommandResource;
	friend Command;
	friend DeviceResource;
	friend Device;
public:
	inline operator bool() const {
		return resource != nullptr;
	}

	void Submit(
		std::span<CommandBufferSubmitInfo const> cmds,
		Fence fence = nullptr,
		SubmitInfo const& info = {}
	) const;
	[[nodiscard]] auto GetInfo() const -> QueueInfo;
	[[nodiscard]] auto GetFamilyIndex() const -> u32;
	[[nodiscard]] auto GetHandle() const -> vk::Queue;
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_QUEUE_HPP_