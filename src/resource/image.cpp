#ifndef VB_USE_STD_MODULE
#include <utility>
#include <vector>
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#ifndef VB_DEV
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif

#include <vulkan/vulkan.h>

#include "vulkan_backend/interface/descriptor.hpp"
#include "vulkan_backend/interface/device.hpp"
#include "vulkan_backend/interface/image.hpp"
#include "vulkan_backend/interface/instance.hpp"
#include "vulkan_backend/log.hpp"
#include "vulkan_backend/util/bits.hpp"
#include "vulkan_backend/vk_result.hpp"
#include "vulkan_backend/macros.hpp"

namespace VB_NAMESPACE {
Image::Image(std::shared_ptr<Device> const& device, ImageInfo const& info)
	: Named(info.name), ResourceBase(device), info(info) {
	VB_LOG_TRACE("[ Create ] type = %s, name = %s", GetResourceTypeName(), GetName());
	Create(info);
}

Image::Image(vk::Image image, vk::ImageView view, Extent3D const& extent, std::string_view name) : vk::Image(image), Named(name),
	view(view), layout(vk::ImageLayout::eUndefined), aspect(vk::ImageAspectFlagBits::eColor), fromSwapchain(true) {
		info.extent = extent;
	}

Image::Image(Image&& other)
	: vk::Image(std::exchange(static_cast<vk::Image&>(other), {})), Named(std::move(other)),
		GpuResource(std::move(other)), ResourceBase<Device>(std::exchange(other.owner, {})),
		view(std::exchange(other.view, {})), allocation(std::exchange(other.allocation, {})),
		layersView(std::move(other.layersView)), layout(std::exchange(other.layout, {})),
		aspect(std::move(other.aspect)), info(std::move(other.info)),
		fromSwapchain(std::exchange(other.fromSwapchain, false)) {}

Image& Image::operator=(Image&& other) {
	if (this != &other) {
		Free();
		vk::Image::operator=(std::exchange(static_cast<vk::Image&>(other), {}));
		Named::operator=(std::move(other));
		GpuResource::operator=(std::exchange(static_cast<GpuResource&>(other), {}));
		ResourceBase::operator=(std::move(other));
		view = std::exchange(other.view, {});
		allocation = std::exchange(other.allocation, {});
		layersView = std::move(other.layersView);
		layout = std::exchange(other.layout, {});
		aspect = std::move(other.aspect);
		info.~ImageInfo();
		new (&info) ImageInfo(std::move(other.info));
		fromSwapchain = std::exchange(other.fromSwapchain, false);
	}
	return *this;
}

Image::~Image() { Free(); }

auto Image::GetFormat() const -> vk::Format { return info.format; }

auto Image::GetResourceTypeName() const -> char const* { return "ImageResource"; }

void Image::Create(ImageInfo const& info) {
	vk::ImageCreateInfo imageInfo{
		.flags =
			info.layers == 6 ? vk::ImageCreateFlagBits::eCubeCompatible : vk::ImageCreateFlags{},
		.imageType	   = vk::ImageType::e2D,
		.format		   = vk::Format(info.format),
		.extent		   = info.extent,
		.mipLevels	   = 1,
		.arrayLayers   = info.layers,
		.samples	   = std::min(info.samples, owner->physical_device->max_samples),
		.tiling		   = vk::ImageTiling::eOptimal,
		.usage		   = info.usage,
		.sharingMode   = vk::SharingMode::eExclusive,
		.initialLayout = vk::ImageLayout::eUndefined,
	};

	VmaAllocationCreateInfo allocInfo = {
		.usage = VMA_MEMORY_USAGE_AUTO,
		.preferredFlags =
			VkMemoryPropertyFlags(info.usage & vk::ImageUsageFlagBits::eTransientAttachment
									  ? VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
									  : 0),
	};

	VB_LOG_TRACE("[ vmaCreateImage ] name = %s, extent = %ux%ux%u, layers = %u", info.name.data(),
				 info.extent.width, info.extent.height, info.extent.depth, info.layers);
	VB_VK_RESULT result = vk::Result(vmaCreateImage(
		owner->vma_allocator, reinterpret_cast<VkImageCreateInfo*>(&imageInfo), &allocInfo,
		reinterpret_cast<VkImage*>(static_cast<vk::Image*>(this)), &allocation, nullptr));
	VB_CHECK_VK_RESULT(vk::Result(result), "Failed to create image!");

	constexpr auto AspectFromFormat = [](vk::Format const f) -> vk::ImageAspectFlags {
		switch (f) {
		case vk::Format::eD24UnormS8Uint: // Invalid, cannot be both stencil and depth,
										  // todo: create separate imageview
			return vk::ImageAspectFlags(vk::ImageAspectFlagBits::eDepth |
										vk::ImageAspectFlagBits::eStencil);
		case vk::Format::eD32Sfloat:
		case vk::Format::eD16Unorm:
			return vk::ImageAspectFlagBits::eDepth;
		default:
			return vk::ImageAspectFlagBits::eColor;
		}
	};
	aspect = AspectFromFormat(info.format);

	vk::ImageViewCreateInfo viewInfo{
		.image	  = *this,
		.viewType = info.layers == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::eCube,
		.format	  = info.format,
		.subresourceRange{.aspectMask	  = vk::ImageAspectFlags(aspect),
						  .baseMipLevel	  = 0,
						  .levelCount	  = 1,
						  .baseArrayLayer = 0,
						  .layerCount	  = info.layers}
	};

	// TODO(nm): Create image view only if usage if Sampled or Storage or other fitting
	result = owner->createImageView(&viewInfo, owner->GetAllocator(), &view);
	VB_CHECK_VK_RESULT(result, "Failed to create image view!");

	if (info.layers > 1) {
		viewInfo.subresourceRange.layerCount = 1;
		layersView.resize(info.layers);
		for (u32 i = 0; i < info.layers; i++) {
			viewInfo.viewType						 = vk::ImageViewType::e2D;
			viewInfo.subresourceRange.baseArrayLayer = i;
			result =
				owner->createImageView(&viewInfo, owner->GetAllocator(), &layersView[i]);
			VB_CHECK_VK_RESULT(result, "Failed to create image view!");
		}
	}
	if (info.binding != ImageInfo::kBindingNone) {
		SetResourceID(owner->descriptor.PopID(info.binding));

		vk::ImageLayout newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		if (info.usage & vk::ImageUsageFlagBits::eSampled) {

			if (TestBits(aspect, vk::ImageAspectFlagBits::eDepth)) [[unlikely]] {
				newLayout = vk::ImageLayout::eDepthReadOnlyOptimal;
			}

			if (TestBits(aspect, vk::ImageAspectFlagBits::eDepth |
									 vk::ImageAspectFlagBits::eStencil)) [[unlikely]] {
				newLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
			}
		}
		if (info.usage & vk::ImageUsageFlagBits::eStorage) {
			newLayout = vk::ImageLayout::eGeneral;
		}

		vk::DescriptorImageInfo descriptorInfo = {
			.sampler	 = owner->GetOrCreateSampler(info.sampler),
			.imageView	 = view,
			.imageLayout = (vk::ImageLayout)newLayout,
		};

		vk::WriteDescriptorSet write = {
			.dstSet			 = owner->descriptor.set,
			.dstBinding		 = info.binding,
			.dstArrayElement = GetResourceID(),
			.descriptorCount = 1,
			.descriptorType	 = owner->descriptor.GetBindingInfo(info.binding).descriptorType,
			.pImageInfo		 = &descriptorInfo,
		};
		owner->updateDescriptorSets(write, {});
	}

	SetDebugUtilsName(info.name.data());
	char const* kViewNameSuffix = "View";
	VB_FORMAT_WITH_SIZE(view_name, 512, "%s%s", info.name.data(), kViewNameSuffix);
	SetDebugUtilsViewName(view_name);
}

void Image::SetDebugUtilsName(char const* name) {
	VB_VK_RESULT result = owner->setDebugUtilsObjectNameEXT({
		.objectType	  = vk::ObjectType::eImage,
		.objectHandle = reinterpret_cast<uint64_t>((static_cast<VkImage>(*this))),
		.pObjectName  = name,
	});
	VB_CHECK_VK_RESULT(result, "Failed to set image debug utils object name!");
}
void Image::SetDebugUtilsViewName(char const* name) {
	VB_VK_RESULT result = owner->setDebugUtilsObjectNameEXT({
		.objectType	  = vk::ObjectType::eImageView,
		.objectHandle = reinterpret_cast<uint64_t>(static_cast<VkImageView>(view)),
		.pObjectName  = name,
	});
	VB_CHECK_VK_RESULT(result, "Failed to set image view debug utils object name!");
}

void Image::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), GetName());
	if (!fromSwapchain) {
		for (VkImageView layerView : layersView) {
			owner->destroyImageView(layerView, owner->GetAllocator());
		}
		layersView.clear();
		owner->destroyImageView(view, owner->GetAllocator());
		vmaDestroyImage(owner->vma_allocator, *this, allocation);
		if (GetResourceID() != kNullID) {
			owner->descriptor.PushID(GetBinding(), GetResourceID());
		}
	}
}

} // namespace VB_NAMESPACE