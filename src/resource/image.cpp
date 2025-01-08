#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <vector>
#else
import std;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include <vulkan/vulkan.h>

#include "image.hpp"
#include "instance.hpp"
#include "device.hpp"
#include "descriptor.hpp"

namespace VB_NAMESPACE {

auto Image::GetResourceID() const -> u32 {
	VB_ASSERT(resource->rid != kNullResourceID, "Invalid image rid");
	return resource->rid;
}

auto Image::GetFormat() const -> Format {
	return resource->format;
}

auto ImageResource::GetType() const -> char const* { return "ImageResource"; }

auto ImageResource::GetInstance() const -> InstanceResource* { return owner->owner; }

void ImageResource::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
	if (!fromSwapchain) {
		for (VkImageView layerView : layersView) {
			vkDestroyImageView(owner->handle, layerView, owner->owner->allocator);
		}
		layersView.clear();
		vkDestroyImageView(owner->handle, view, owner->owner->allocator);
		vmaDestroyImage(owner->vmaAllocator, handle, allocation);
		if (rid != kNullResourceID) {
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

} // namespace