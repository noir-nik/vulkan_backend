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

#ifndef VB_USE_VMA_MODULE
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

class Image : public vk::Image, public Named, public ResourceBase<Device> {
  public:
	// No-op constructor
	Image() = default;

	// RAII constructor, calls Create(info)
	Image(Device& device, ImageInfo const& info);

	// From swapchain
	Image(vk::Image image, vk::ImageView view, Extent3D const& extent, std::string_view name = "");

	// Calls Free
	~Image();

	// Move constructor
	Image(Image&& other);

	// Move assignment
	Image& operator=(Image&& other);

	// Get is valid
	using vk::Image::operator bool;

	// Create with result checked
	auto Create(Device& device, ImageInfo const& info) -> vk::Result;

	// Manually free resources, safe to call multiple times
	void Free() override;

	inline auto GetFormat() const -> vk::Format { return format; }
	inline auto GetView() -> vk::ImageView& { return view; }
	inline auto GetView() const -> vk::ImageView const& { return view; }
	inline auto GetAllocation() const -> VmaAllocation { return allocation; }
	inline auto GetLayout() const -> vk::ImageLayout { return layout; }
	inline auto GetAspect() const -> vk::ImageAspectFlags { return aspect; }
	inline auto GetExtent() const -> vk::Extent3D { return extent; }
	inline auto GetUsage() const -> vk::ImageUsageFlags { return usage; }

	// Do not call
	inline void SetLayout(vk::ImageLayout const layout) { this->layout = layout; }

	inline auto IsFromSwapchain() const -> bool { return fromSwapchain; }

	auto GetDevice() const -> Device& { return *GetOwner(); }
	auto GetResourceTypeName() const -> char const* override;

	// Utility
	void SetDebugUtilsName(std::string_view const name);
	void SetDebugUtilsViewName(std::string_view const name);

	static inline auto MakeCreateInfo(vk::Format format, Extent3D const& extent, vk::ImageUsageFlags usage)
		-> vk::ImageCreateInfo;

  private:
	vk::ImageView view;
	VmaAllocation allocation;

	vk::ImageAspectFlags aspect;
	// From Create info
	vk::ImageLayout     layout;
	vk::Extent3D        extent;
	vk::Format          format;
	vk::ImageUsageFlags usage;

	bool fromSwapchain = false;
};

class BindlessImage : public Image, public BindlessResourceBase {
  public:
	// No-op constructor
	BindlessImage() = default;

	// RAII constructor, calls Create
	BindlessImage(Device& device, BindlessDescriptor& descriptor, BindlessImageInfo const& info);

	// Move constructor
	BindlessImage(BindlessImage&& other);

	// Move assignment
	BindlessImage& operator=(BindlessImage&& other);

	// Calls Free
	~BindlessImage();

	// Create Manually
	vk::Result Create(Device& device, BindlessDescriptor& descriptor, BindlessImageInfo const& info);

	// Manually free resources, safe to call multiple times
	void Free() override;
};

} // namespace VB_NAMESPACE
