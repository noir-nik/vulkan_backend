#ifndef VULKAN_BACKEND_RESOURCE_IMAGE_HPP_
#define VULKAN_BACKEND_RESOURCE_IMAGE_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#else
import std;
#endif

#include <vulkan/vulkan.h>

#include <vulkan_backend/fwd.hpp>
#include "vulkan_backend/structs.hpp"
#include "base.hpp"
#include "device.hpp"
#include "descriptor.hpp"
#include "../util.hpp"

namespace VB_NAMESPACE {
struct ImageResource : ResourceBase<DeviceResource> {
	u32 rid = NullRID;

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

	inline auto GetInstance() -> InstanceResource* { return owner->owner; }
	auto GetType() -> char const* override {  return "ImageResource"; }
	void Free() override {
		VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
		if (!fromSwapchain) {
			for (VkImageView layerView : layersView) {
				vkDestroyImageView(owner->handle, layerView, owner->owner->allocator);
			}
			layersView.clear();
			vkDestroyImageView(owner->handle, view, owner->owner->allocator);
			vmaDestroyImage(owner->vmaAllocator, handle, allocation);
			if (rid != NullRID) {
				if (usage & ImageUsage::eStorage) {
					owner->descriptor.resource->PushID(DescriptorType::eStorageImage, rid);
				}
				if (usage & ImageUsage::eSampled) {
					owner->descriptor.resource->PushID(DescriptorType::eCombinedImageSampler, rid);
				}
				// for (ImTextureID imguiRID : imguiRIDs) {
					// ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)imguiRID);
				// }
			}
		}
	}
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_RESOURCE_IMAGE_HPP_