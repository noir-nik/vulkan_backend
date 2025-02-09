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

#include "vulkan_backend/classes/no_copy_no_move.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/command/structs.hpp"
#include "vulkan_backend/interface/queue/info.hpp"
#include "vulkan_backend/types.hpp"


VB_EXPORT
namespace VB_NAMESPACE {
// Queue handle
class Queue : public vk::Queue {
  public:
	void Submit(std::span<vk::CommandBufferSubmitInfo const> cmds, vk::Fence fence = nullptr,
				SubmitInfo const& info = {}) const;
	void Wait() const;
	auto GetFamilyIndex() const -> u32;
	auto GetIndex() const -> u32;
	auto GetFlags() const -> vk::QueueFlags;
	
  private:
	// Queue(vk::Queue queue, Device& device, u32 family, u32 index, vk::QueueFlags flags);
	using vk::Queue::operator=;
	friend Swapchain;
	friend Command;
	friend Device;
	Device*		   device = nullptr;
	u32			   family = ~0u;
	u32			   index  = ~0u;
	vk::QueueFlags flags  = {};
};
} // namespace VB_NAMESPACE
