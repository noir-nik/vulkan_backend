#ifndef VULKAN_BACKEND_RESOURCE_SWAPCHAIN_HPP_
#define VULKAN_BACKEND_RESOURCE_SWAPCHAIN_HPP_

#ifndef VB_USE_STD_MODULE
#else
import std;
#endif

#include <vulkan/vulkan.h>

#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/swapchain.hpp"
#include "vulkan_backend/interface/command.hpp"
#include "base.hpp"

namespace VB_NAMESPACE {
struct SwapChainResource: ResourceBase<DeviceResource> {
	VkSwapchainKHR                  handle  = VK_NULL_HANDLE;
	VkSurfaceCapabilitiesKHR        surface_capabilities;
	std::vector<VkSemaphore>        image_available_semaphores;
	std::vector<VkSemaphore>        render_finished_semaphores;
	std::vector<VkPresentModeKHR>   available_present_modes;
	std::vector<VkSurfaceFormatKHR> available_surface_formats;

	std::vector<Command> commands;
	std::vector<Image> images;

	u32 current_frame = 0;
	u32 current_image_index = 0;
	bool dirty = true;

	SwapchainInfo init_info = {{}, {}, {}};
	
	Surface surface;
	Extent2D extent;

	inline auto GetImageAvailableSemaphore() -> VkSemaphore {
		return image_available_semaphores[current_frame];
	}
	inline auto GetRenderFinishedSemaphore() -> VkSemaphore {
		return render_finished_semaphores[current_frame];
	}
	auto SupportsFormat(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features) -> bool;
	void CreateSurfaceFormats();
	auto ChooseSurfaceFormat(SurfaceFormat const& desired_surface_format) -> SurfaceFormat;
	auto ChoosePresentMode(PresentMode const& desired_present_mode) -> PresentMode;
	auto ChooseExtent(Extent2D const& desired_extent) -> Extent2D;
	void CreateSwapchain();
	void CreateImages();
	void CreateSemaphores();
	void CreateCommands(u32 queueFamilyindex);
	void Create(SwapchainInfo const& info);
	// void ReCreate(u32 width, u32 height);
		
	auto GetInstance() const -> InstanceResource* override;
	auto GetType() const -> char const* override;
	using ResourceBase::ResourceBase;
	void Free() override;
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_RESOURCE_SWAPCHAIN_HPP_