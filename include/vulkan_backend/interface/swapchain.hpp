#ifndef VULKAN_BACKEND_SWAPCHAIN_HPP_
#define VULKAN_BACKEND_SWAPCHAIN_HPP_

#ifndef VB_USE_STD_MODULE
#include <memory>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "../fwd.hpp"

VB_EXPORT
namespace VB_NAMESPACE {	
struct ImGuiInitInfo {
	vk::Instance                      Instance;
	vk::PhysicalDevice                PhysicalDevice;
	vk::Device                        Device;
	vk::DescriptorPool                DescriptorPool;
	u32                               MinImageCount;
	u32                               ImageCount;
	vk::PipelineCache                 PipelineCache;
	bool                              UseDynamicRendering;
	const vk::AllocationCallbacks*    Allocator;
};

struct SwapchainInfo {
	u32 static constexpr inline kFramesInFlight = 3;
	u32 static constexpr inline kAdditionalImages  = 0;
	Surface const&  surface;
	Extent2D const& extent;
	u32 const&      queueFamilyindex; // for command buffers
	bool            destroy_surface   = false;
	u32             frames_in_flight  = kFramesInFlight;
	u32             additional_images = kAdditionalImages;
	// Preferred color format, not guaranteed, get actual format after creation with GetFormat()
	Format          preferred_format  = Format::eR8G8B8A8Unorm;
	ColorSpace      color_space       = ColorSpace::eSrgbNonlinear;
	PresentMode     present_mode      = PresentMode::eMailbox;
};

class Swapchain {
public:
	bool AcquireImage();
	void SubmitAndPresent(Queue const& submit, Queue const& present);
	void Recreate(u32 width, u32 height);

	auto GetCurrentImage()  -> Image&;
	auto GetCommandBuffer() -> Command&;
	auto GetDirty()         -> bool;
	auto GetFormat()        -> Format;
	auto GetImGuiInfo()     -> ImGuiInitInfo;
private:
	std::shared_ptr<SwapChainResource> resource;
	friend Device;
};

} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_SWAPCHAIN_HPP_