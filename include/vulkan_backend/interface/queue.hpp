#pragma once

#ifndef VB_USE_STD_MODULE
#include <span>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/types.hpp"
#include "vulkan_backend/classes/no_copy_no_move.hpp"
#include "vulkan_backend/structs/command.hpp"
#include "vulkan_backend/interface/info/queue.hpp"

VB_EXPORT
namespace VB_NAMESPACE {	
// Queue handle
class Queue : public vk::Queue {
public:
	void Submit(
		std::span<vk::CommandBufferSubmitInfo const> cmds,
		vk::Fence                                    fence = nullptr,
		SubmitInfo const&                            info = {}
	) const;
	auto GetInfo() const -> QueueInfo;
	auto GetFamilyIndex() const -> u32;
	auto GetIndex() const -> u32;

	using vk::Queue::operator=;
	Queue(vk::Queue queue, Device* device, u32 family, u32 index, QueueInfo info);

private:
	friend Swapchain;
	friend Command;
	friend Device;
	Device*	  device = nullptr;
	u32		  family;
	u32		  index;
	QueueInfo info;
};
} // namespace VB_NAMESPACE
