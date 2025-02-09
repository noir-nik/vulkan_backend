#pragma once

#ifndef VB_USE_STD_MODULE
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/classes/base.hpp"
#include "vulkan_backend/classes/structs.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/command/command.hpp"
#include "vulkan_backend/interface/image/image.hpp"
#include "vulkan_backend/interface/queue/queue.hpp"
#include "vulkan_backend/interface/swapchain/info.hpp"
#include "vulkan_backend/types.hpp"


VB_EXPORT
namespace VB_NAMESPACE {
class Swapchain : public vk::SwapchainKHR, public Named, public ResourceBase<Device> {
  public:
	Swapchain(Device& device, SwapchainInfo const& info);
	Swapchain(Swapchain&& other) noexcept;
	Swapchain& operator=(Swapchain&& other) noexcept;
	~Swapchain();

	void Create(Device& device, SwapchainInfo const& info);
	
	bool AcquireNextImage();
	void SubmitAndPresent(vk::Queue const& submit, vk::Queue const& present);
	void Recreate(u32 width, u32 height);

	auto GetCurrentImage() -> Image&;
	auto GetCommandBuffer() -> Command&;
	auto GetDirty() -> bool;
	auto GetFormat() -> vk::Format;
	auto GetExtent() const -> vk::Extent2D;

	inline auto GetImageAvailableSemaphore() -> vk::Semaphore {
		return image_available_semaphores[current_frame];
	}
	inline auto GetRenderFinishedSemaphore() -> vk::Semaphore {
		return render_finished_semaphores[current_frame];
	}
	
	auto GetDevice() const -> Device& { return *GetOwner(); }
	auto GetResourceTypeName() const -> char const* override;

  private:
	auto SupportsFormat(vk::Format format, vk::ImageTiling tiling, vk::FormatFeatureFlags features) -> bool;
	void CreateSurfaceFormats();
	auto ChooseSurfaceFormat(vk::SurfaceFormatKHR const& desired_surface_format) -> vk::SurfaceFormatKHR;
	auto ChoosePresentMode(vk::PresentModeKHR const& desired_present_mode) -> vk::PresentModeKHR;
	auto ChooseExtent(vk::Extent2D const& desired_extent) -> vk::Extent2D;
	void CreateSwapchain();
	void CreateImages();
	void CreateSemaphores();
	void CreateCommands(u32 queueFamilyindex);

	void Free() override;

	vk::SurfaceCapabilitiesKHR		  surface_capabilities;
	std::vector<vk::Semaphore>		  image_available_semaphores;
	std::vector<vk::Semaphore>		  render_finished_semaphores;
	std::vector<vk::PresentModeKHR>	  available_present_modes;
	std::vector<vk::SurfaceFormatKHR> available_surface_formats;
	std::vector<Command>			  commands;
	std::vector<Image>				  images;
	u32								  current_frame		  = 0;
	u32								  current_image_index = 0;
	bool							  dirty				  = true;
	SwapchainInfo					  info;
};

} // namespace VB_NAMESPACE
