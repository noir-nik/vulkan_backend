#pragma once

#ifndef VB_USE_STD_MODULE
#include <string_view>
#include <vector>

#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#ifndef VB_DEV
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif

#include "vulkan_backend/classes/base.hpp"
#include "vulkan_backend/classes/gpu_resource.hpp"
#include "vulkan_backend/classes/structs.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/image/info.hpp"
#include "vulkan_backend/types.hpp"

VB_EXPORT
namespace VB_NAMESPACE {

// Image handle
using ImageRef = Image*;

class Image : public vk::Image, public Named, public GpuResource, public ResourceBase<Device> {
  public:
	// Creates resources
	Image(Device& device, ImageInfo const& info);
	// From swapchain
	Image(vk::Image image, vk::ImageView view, Extent3D const& extent, std::string_view name = "");
	// Frees all resources
	~Image();

	Image(Image&& other);
	Image& operator=(Image&& other);
	operator bool() const { return vk::Image::operator bool(); }

	auto GetFormat() const -> vk::Format;
	auto GetResourceTypeName() const -> char const* override;

	// Utility
	void SetDebugUtilsName(char const* name);
	void SetDebugUtilsViewName(char const* name);
	
	inline auto GetBinding() const -> u32 { return info.binding; }
	inline void SetBinding(u32 binding) { info.binding = binding; }
  private:
  	friend Swapchain;
	friend Device;
	friend Command;
	friend RenderingInfo;
	Image() = default;
	void Create(ImageInfo const& info);
	void Free() override;

	vk::ImageView			   view;
	VmaAllocation			   allocation;
	std::vector<vk::ImageView> layersView;
	vk::ImageLayout			   layout;
	vk::ImageAspectFlags	   aspect;
	ImageInfo				   info;
	bool					   fromSwapchain = false;
};
} // namespace VB_NAMESPACE
