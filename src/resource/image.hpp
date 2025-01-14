#ifndef VULKAN_BACKEND_RESOURCE_IMAGE_HPP_
#define VULKAN_BACKEND_RESOURCE_IMAGE_HPP_

#ifndef VB_USE_STD_MODULE
#include <vector>
#else
import std;
#endif

#include <vulkan/vulkan.h>

#include <vk_mem_alloc.h>

#include "vulkan_backend/structs.hpp"
#include "base.hpp"

namespace VB_NAMESPACE {
struct ImageResource : ResourceBase<DeviceResource> {
	u32 rid = kNullResourceID;

	VkImage handle;
	VkImageView view;
	VmaAllocation allocation;
	bool fromSwapchain = false;
	SampleCount samples = SampleCount::e1;
	std::vector<VkImageView> layersView;
	Extent3D extent{};
	ImageUsageFlags usage;
	Format format;
	ImageLayout layout;
	ImageAspectFlags aspect;
	u32 layers = 1;

	using ResourceBase::ResourceBase;

	auto GetType() const -> char const* override;
	auto GetInstance() const -> InstanceResource* override;
	void Free() override;
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_RESOURCE_IMAGE_HPP_