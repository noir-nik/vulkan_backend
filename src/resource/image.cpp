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

#ifndef VB_USE_VMA_MODULE
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif

#include <vulkan/vulkan.h>

#include "vulkan_backend/constants/constants.hpp"
#include "vulkan_backend/interface/descriptor/descriptor.hpp"
#include "vulkan_backend/interface/device/device.hpp"
#include "vulkan_backend/interface/image/image.hpp"
#include "vulkan_backend/interface/physical_device/physical_device.hpp"
#include "vulkan_backend/log.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/util/bits.hpp"
#include "vulkan_backend/util/format.hpp"
#include "vulkan_backend/vk_result.hpp"

namespace VB_NAMESPACE {
Image::Image(Device& device, ImageInfo const& info) { Create(device, info); }

Image::Image(vk::Image image, vk::ImageView view, Extent3D const& extent, std::string_view name)
	: vk::Image(image), Named(name), view(view), layout(vk::ImageLayout::eUndefined), aspect(vk::ImageAspectFlagBits::eColor),
	  fromSwapchain(true) {
	this->extent = extent;
}

Image::Image(Image&& other)
	: vk::Image(std::exchange(static_cast<vk::Image&>(other), {})), Named(std::move(other)),
	  ResourceBase<Device>(std::move(other)), view(std::exchange(other.view, {})),
	  allocation(std::exchange(other.allocation, {})), layout(std::move(other.layout)), aspect(std::move(other.aspect)),
	  extent(std::move(other.extent)), format(std::move(other.format)), usage(std::move(other.usage)),
	  fromSwapchain(std::move(other.fromSwapchain)) {}

Image& Image::operator=(Image&& other) {
	if (this != &other) {
		Free();
		vk::Image::operator=(std::exchange(static_cast<vk::Image&>(other), {}));
		Named::operator=(std::move(other));
		ResourceBase::operator=(std::move(other));
		view       = std::exchange(other.view, {});
		allocation = std::exchange(other.allocation, {});
		layout     = std::move(other.layout);
		aspect     = std::move(other.aspect);

		extent        = std::move(other.extent);
		format        = std::move(other.format);
		usage         = std::move(other.usage);
		fromSwapchain = std::move(other.fromSwapchain);
	}
	return *this;
}

Image::~Image() { Free(); }

auto Image::GetResourceTypeName() const -> char const* { return "ImageResource"; }

auto Image::Create(Device& device, ImageInfo const& info) -> vk::Result {
	VB_LOG_TRACE("[ Create ] type = %s, name = %s", GetResourceTypeName(), GetName().data());
	ResourceBase::SetOwner(&device);
	SetName(info.name);
	this->extent = info.create_info.extent;
	this->format = info.create_info.format;
	this->usage  = info.create_info.usage;
	this->layout = info.create_info.initialLayout;
	this->aspect = info.aspect;

	VmaAllocationCreateInfo allocInfo = {
		.usage          = VMA_MEMORY_USAGE_AUTO,
		.preferredFlags = VkMemoryPropertyFlags(
			info.create_info.usage & vk::ImageUsageFlagBits::eTransientAttachment ? VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT : 0),
	};

	VB_LOG_TRACE("[ vmaCreateImage ] extent = %ux%ux%u, layers = %u name = %s", info.create_info.extent.width, info.create_info.extent.height,
				 info.create_info.extent.depth, info.create_info.arrayLayers, detail::FormatName(info.name).data());
	VB_VK_RESULT result =
		vk::Result(vmaCreateImage(GetDevice().GetVmaAllocator(), reinterpret_cast<VkImageCreateInfo const*>(&info), &allocInfo,
								  reinterpret_cast<VkImage*>(static_cast<vk::Image*>(this)), &allocation, nullptr));
	VB_VERIFY_VK_RESULT(vk::Result(result), info.check_vk_results, "Failed to create image!", {});

	vk::ImageViewCreateInfo viewInfo{
		.image    = *this,
		.viewType = info.create_info.arrayLayers == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::eCube,
		.format   = info.create_info.format,
		.subresourceRange{.aspectMask     = aspect,
						  .baseMipLevel   = 0,
						  .levelCount     = info.create_info.mipLevels,
						  .baseArrayLayer = 0,
						  .layerCount     = info.create_info.arrayLayers}
    };

	// TODO(nm): Create image view only if usage if Sampled or Storage or other fitting
	result = GetDevice().createImageView(&viewInfo, GetDevice().GetAllocator(), &view);
	VB_VERIFY_VK_RESULT(result, info.check_vk_results, "Failed to create image view!",
						{ vmaDestroyImage(GetDevice().GetVmaAllocator(), *this, allocation); });

	if (GetDevice().GetInstance().IsDebugUtilsEnabled()) {
		SetDebugUtilsName(info.name.data());
		constexpr char const* kViewNameSuffix = "View";
		if (info.name.empty()) {
			SetDebugUtilsViewName(kViewNameSuffix);
		} else {
			char view_name[kMaxObjectNameSize];
			std::snprintf(view_name, kMaxObjectNameSize - 1, "%s%s", info.name.data(), kViewNameSuffix);
			SetDebugUtilsViewName(view_name);
		}
	}

	return vk::Result::eSuccess;
}

void Image::SetDebugUtilsName(std::string_view const name) {
	GetDevice().SetDebugUtilsName(vk::ObjectType::eImage, static_cast<vk::Image*>(this), name.data());
}

void Image::SetDebugUtilsViewName(std::string_view const name) {
	GetDevice().SetDebugUtilsName(vk::ObjectType::eImageView, &view, name.data());
}

void Image::Free() {
	if (!vk::Image::operator bool())
		return;
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), detail::FormatName(GetName()).data());
	if (!fromSwapchain) {
		GetDevice().destroyImageView(view, GetDevice().GetAllocator());
		vmaDestroyImage(GetDevice().GetVmaAllocator(), *this, allocation);
		vk::Image::operator=(nullptr);
		view = vk::ImageView{};
	}
}

BindlessImage::BindlessImage(Device& device, BindlessDescriptor& descriptor, BindlessImageInfo const& info) { Create(device, descriptor, info); }

BindlessImage::BindlessImage(BindlessImage&& other) {
	Image::operator=(std::move(other));
	BindlessResourceBase::operator=(std::move(other));
}

BindlessImage& BindlessImage::operator=(BindlessImage&& other) {
	if (this != &other) {
		Image::operator=(std::move(other));
		BindlessResourceBase::operator=(std::move(other));
	}
	return *this;
}

BindlessImage::~BindlessImage() { Free(); }

auto BindlessImage::Create(Device& device, BindlessDescriptor& descriptor, BindlessImageInfo const& info) -> vk::Result {
	VB_VERIFY_VK_RESULT(vb::Image::Create(device, info.image_info), false, "Failed to create image!", {});

	// Bind to get resource id
	BindlessResourceBase::Bind(descriptor, info.binding);
	
	// Update bindless descriptor
	vk::DescriptorImageInfo descriptorInfo = {
		.sampler     = info.sampler,
		.imageView   = GetView(),
		.imageLayout = info.layout,
	};

	vk::WriteDescriptorSet write = {
		.dstSet          = descriptor.GetSet(),
		.dstBinding      = info.binding,
		.dstArrayElement = GetResourceID(),
		.descriptorCount = 1,
		.descriptorType  = descriptor.GetBindingInfo(info.binding).descriptorType,
		.pImageInfo      = &descriptorInfo,
	};
	GetDevice().updateDescriptorSets(write, {});
	return vk::Result::eSuccess;
}

void BindlessImage::Free() {
	if (BindlessResourceBase::IsBound()) {
		BindlessResourceBase::Release();
	}
	Image::Free();
}

} // namespace VB_NAMESPACE