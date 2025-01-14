#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <limits>
#else
import std;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/interface/swapchain.hpp"
#include "swapchain.hpp"
#include "device.hpp"
#include "instance.hpp"
#include "image.hpp"
#include "command.hpp"
#include "../macros.hpp"

namespace VB_NAMESPACE {
bool Swapchain::AcquireImage() {
	auto GetInstance = [this]() -> InstanceResource* { return this->resource->GetInstance(); };
	auto result = vkAcquireNextImageKHR(
		resource->owner->handle,
		resource->handle,
		UINT64_MAX,
		resource->GetImageAvailableSemaphore(),
		VK_NULL_HANDLE,
		&resource->current_image_index
	);

	switch (result) {
	[[likely]]
	case VK_SUCCESS:
		return true;
	case VK_ERROR_OUT_OF_DATE_KHR:
		VB_LOG_WARN("AcquireImage: Out of date");
		resource->dirty = true;
		return false;
	case VK_SUBOPTIMAL_KHR:
		return false;
	default:
		VB_CHECK_VK_RESULT(resource->owner->owner->init_info.checkVkResult, result, "Failed to acquire swap chain image!");
		return false;
	}
}

// EndCommandBuffer + vkQueuePresentKHR
void Swapchain::SubmitAndPresent(Queue const& submit, Queue const& present) {
	auto& current_image_index = resource->current_image_index;
	auto& cmd = GetCommandBuffer();
	auto GetInstance = [this]() -> InstanceResource* { return this->resource->GetInstance(); };

	cmd.End();
	cmd.QueueSubmit( submit, {
		.waitSemaphoreInfos   = {{{.semaphore = Semaphore(resource->GetImageAvailableSemaphore())}}},
		.signalSemaphoreInfos = {{{.semaphore = Semaphore(resource->GetRenderFinishedSemaphore())}}}
	});

	VkSemaphore present_wait = resource->GetRenderFinishedSemaphore();
	VkPresentInfoKHR presentInfo{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1, // TODO(nm): pass with info
		.pWaitSemaphores = &present_wait,
		.swapchainCount = 1,
		.pSwapchains = &resource->handle,
		.pImageIndices = &current_image_index,
		.pResults = nullptr,
	};

	auto result = vkQueuePresentKHR(present.resource->handle, &presentInfo);

	switch (result) {
	case VK_SUCCESS: break;
	case VK_ERROR_OUT_OF_DATE_KHR:
	case VK_SUBOPTIMAL_KHR:
		VB_LOG_WARN("vkQueuePresentKHR: %s", StringFromVkResult(vk::Result(result)));
		resource->dirty = true;
		return;
	default:
		VB_CHECK_VK_RESULT(resource->owner->owner->init_info.checkVkResult, result, "Failed to present swap chain image!"); 
		break;
	}

	resource->current_frame = (resource->current_frame + 1) % resource->init_info.frames_in_flight;

}

void Swapchain::Recreate(u32 width, u32 height) {
	VB_ASSERT(width > 0 && height > 0, "Window size is 0, swapchain NOT to be recreated");
	for (auto& cmd: resource->commands) {
		vkWaitForFences(resource->owner->handle, 1, &cmd.resource->fence, VK_TRUE, std::numeric_limits<u32>::max());
	}

	for (auto& image : resource->images) {
		vkDestroyImageView(resource->owner->handle, image.resource->view, resource->owner->owner->allocator);
	}
	resource->images.clear();
	resource->CreateSurfaceFormats();
	resource->extent = resource->ChooseExtent({width, height});
	auto [format, colorSpace] = resource->ChooseSurfaceFormat({
		resource->init_info.preferred_format, 
		resource->init_info.color_space
	});
	resource->init_info.preferred_format = format;
	resource->init_info.color_space = colorSpace;
	resource->init_info.present_mode = resource->ChoosePresentMode(resource->init_info.present_mode);
	resource->CreateSwapchain();
	resource->CreateImages();
}

auto Swapchain::GetCurrentImage() -> Image& {
	return resource->images[resource->current_image_index];
}

auto Swapchain::GetCommandBuffer() -> Command& {
	return resource->commands[resource->current_frame];
}

bool Swapchain::GetDirty() {
	return resource->dirty;
}

auto Swapchain::GetFormat() -> Format {
	return resource->init_info.preferred_format;
}

auto Swapchain::GetImGuiInfo() -> ImGuiInitInfo {
	ImGuiInitInfo init_info{
		.Instance            = resource->owner->owner->handle,
		.PhysicalDevice      = resource->owner->physicalDevice->handle,
		.Device              = resource->owner->handle,
		.DescriptorPool      = resource->owner->imguiDescriptorPool,
		.MinImageCount       = std::max(resource->surface_capabilities.minImageCount, 2u),
		.ImageCount          = (u32)resource->images.size(),
		.PipelineCache       = resource->owner->pipelineCache,
		.UseDynamicRendering = true,
		.Allocator           = reinterpret_cast<vk::AllocationCallbacks const*>(resource->owner->owner->allocator),
	};
	return init_info;
}


bool SwapChainResource::SupportsFormat(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features) {
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties(owner->physicalDevice->handle, format, &props);

	if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
		return true;
	} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
		return true;
	}

	return false;
}

auto SwapChainResource::ChoosePresentMode(PresentMode const& desired_present_mode) -> PresentMode {
	for (auto const& mode : available_present_modes) {
		if (mode == static_cast<VkPresentModeKHR>(desired_present_mode)) {
			return desired_present_mode;
		}
	}
	VB_LOG_WARN("Preferred present mode not available!");
	// FIFO is guaranteed to be available
	return PresentMode::eFifo;
}

auto SwapChainResource::ChooseExtent(Extent2D const& desired_extent) -> Extent2D {
	if (this->surface_capabilities.currentExtent.width != UINT32_MAX) {
		return {
			surface_capabilities.currentExtent.width,
			surface_capabilities.currentExtent.height,
		};
	}
	return Extent2D {
		.width = std::max(
			this->surface_capabilities.minImageExtent.width,
			std::min(this->surface_capabilities.maxImageExtent.width, desired_extent.width)
		),
		.height = std::max(
			this->surface_capabilities.minImageExtent.height,
			std::min(this->surface_capabilities.maxImageExtent.height, desired_extent.height)
		)
	};
}

void SwapChainResource::CreateSurfaceFormats() {
	// get capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(owner->physicalDevice->handle, surface, &surface_capabilities);

	// get surface formats
	u32 formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(owner->physicalDevice->handle, surface, &formatCount, nullptr);
	if (formatCount != 0) {
		available_surface_formats.clear();
		available_surface_formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(owner->physicalDevice->handle, surface, &formatCount, available_surface_formats.data());
	}

	// get present modes
	u32 modeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(owner->physicalDevice->handle, surface, &modeCount, nullptr);
	if (modeCount != 0) {
		available_present_modes.clear();
		available_present_modes.resize(modeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(owner->physicalDevice->handle, surface, &modeCount, available_present_modes.data());
	}

	// set this device as not suitable if no surface formats or present modes available
	bool suitable = (modeCount > 0);
	suitable &= (formatCount > 0);
	VB_ASSERT(suitable, "Selected device invalidated after surface update.");
}

auto SwapChainResource::ChooseSurfaceFormat(SurfaceFormat const& desired_surface_format) -> SurfaceFormat {
	for (auto const& availableFormat : available_surface_formats) {
		if (availableFormat.format == VkFormat(desired_surface_format.format) &&
			availableFormat.colorSpace == VkColorSpaceKHR(desired_surface_format.colorSpace)) {
			return {Format(availableFormat.format), ColorSpace(availableFormat.colorSpace)};
		}
	}
	VB_LOG_WARN("Preferred surface format not available!");
	return {Format(available_surface_formats[0].format), ColorSpace(available_surface_formats[0].colorSpace)};
}

void SwapChainResource::CreateSwapchain() {
	auto const& capabilities = surface_capabilities;

	// acquire additional images to prevent waiting for driver's internal operations
	u32 image_count = init_info.frames_in_flight + init_info.additional_images;

	if (image_count < capabilities.minImageCount) {
		VB_LOG_WARN("Querying less images %u than the necessary: %u", image_count, capabilities.minImageCount);
		image_count = capabilities.minImageCount;
	}

	// prevent exceeding the max image count
	if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
		VB_LOG_WARN("Querying more images than supported. imageCount set to maxImageCount.");
		image_count = capabilities.maxImageCount;
	}

	// Create swapchain
	VkSwapchainCreateInfoKHR createInfo {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = image_count,
		.imageFormat = VkFormat(init_info.preferred_format),
		.imageColorSpace = static_cast<VkColorSpaceKHR>(init_info.color_space),
		.imageExtent = {extent.width, extent.height},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT  // if we want to render to a separate image first to perform post-processing
						| VK_IMAGE_USAGE_TRANSFER_DST_BIT, // we should add this image usage
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, // don't support different graphics and present family
		.queueFamilyIndexCount = 0,     // only when imageSharingMode is VK_SHARING_MODE_CONCURRENT
		.pQueueFamilyIndices = nullptr, // only when imageSharingMode is VK_SHARING_MODE_CONCURRENT
		.preTransform = capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, // blend the images with other windows in the window system
		.presentMode = static_cast<VkPresentModeKHR>(init_info.present_mode),
		.clipped = true, // clip pixels behind our window
		.oldSwapchain = handle,
	};

	VB_VK_RESULT result = vkCreateSwapchainKHR(owner->handle, &createInfo, owner->owner->allocator, &handle);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create swap chain!");	
	vkDestroySwapchainKHR(owner->handle, createInfo.oldSwapchain, owner->owner->allocator);
}

void SwapChainResource::CreateImages() {
	u32 imageCount;
	vkGetSwapchainImagesKHR(owner->handle, handle, &imageCount, nullptr);
	VB_VLA(VkImage, vk_images, imageCount);
	vkGetSwapchainImagesKHR(owner->handle, handle, &imageCount, vk_images.data());

	// Create image views
	images.clear();
	images.reserve(vk_images.size());
	for (auto vk_image: vk_images) {
		images.emplace_back();
		auto& image = images.back();
		image.resource = MakeResource<ImageResource>(owner, "SwapChain Image " + std::to_string(images.size()));
		image.resource->fromSwapchain = true;
		image.resource->handle  = vk_image;
		image.resource->extent = {init_info.extent.width, init_info.extent.height, 1};
		image.resource->layout = ImageLayout::eUndefined;
		image.resource->aspect = Aspect::eColor;

		VkImageViewCreateInfo viewInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = vk_image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = VkFormat(init_info.preferred_format),
			.subresourceRange {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};

		VB_VK_RESULT result = vkCreateImageView(owner->handle, &viewInfo, owner->owner->allocator, &image.resource->view);
		VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create SwapChain image view!");

		// Add debug names
		if (owner->owner->init_info.validation_layers) {
			owner->SetDebugUtilsObjectName({
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VkObjectType::VK_OBJECT_TYPE_IMAGE,
				.objectHandle = reinterpret_cast<uint64_t>(image.resource->handle),
				.pObjectName = image.resource->name.data(),
			});
			owner->SetDebugUtilsObjectName({
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VkObjectType::VK_OBJECT_TYPE_IMAGE_VIEW,
				.objectHandle = reinterpret_cast<uint64_t>(image.resource->view),
				.pObjectName = (image.resource->name + " View").data(),
			});
		}
	}
}

void SwapChainResource::CreateSemaphores() {
	image_available_semaphores.resize(init_info.frames_in_flight);
	render_finished_semaphores.resize(init_info.frames_in_flight);

	VkSemaphoreCreateInfo semaphoreInfo{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	for (size_t i = 0; i < init_info.frames_in_flight; ++i) {
		VB_VK_RESULT
		result = vkCreateSemaphore(owner->handle, &semaphoreInfo, nullptr, &image_available_semaphores[i]);
		VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create semaphore!");
		result = vkCreateSemaphore(owner->handle, &semaphoreInfo, nullptr, &render_finished_semaphores[i]);
		VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create semaphore!");
	}
}

void SwapChainResource::CreateCommands(u32 queueFamilyindex) {
	commands.resize(init_info.frames_in_flight);
	for (auto& cmd: commands) {
		cmd.resource = MakeResource<CommandResource>(owner, "Swapchain Command Buffer");
		cmd.resource->Create(queueFamilyindex);
	}
}

void SwapChainResource::Create(SwapchainInfo const& info) {
	// this is a necessary hackery
	init_info.~SwapchainInfo();
	new(&init_info) SwapchainInfo(info);
	surface = info.surface;
	CreateSurfaceFormats();
	auto [format, colorSpace] = ChooseSurfaceFormat({
		init_info.preferred_format,
		init_info.color_space
	});
	init_info.preferred_format = format;
	init_info.color_space = colorSpace;
	init_info.present_mode = ChoosePresentMode(init_info.present_mode);
	extent = ChooseExtent(init_info.extent);
	CreateSwapchain();
	CreateImages();
	CreateSemaphores();
	CreateCommands(info.queueFamilyindex);
	current_frame = 0;
	current_image_index = 0;
	dirty = false;
	VB_LOG_TRACE("Created Swapchain");
}

auto SwapChainResource::GetInstance() const -> InstanceResource* {
	return owner->owner;
}

auto SwapChainResource::GetType() const -> char const* {
	return "SwapChainResource";
}

void SwapChainResource::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
	for (auto& cmd: commands) {
		vkWaitForFences(owner->handle, 1, &cmd.resource->fence, VK_TRUE, std::numeric_limits<u32>::max());
	}

	for (auto& image : images) {
		vkDestroyImageView(owner->handle, image.resource->view, owner->owner->allocator);
	}
	images.clear();

	for (size_t i = 0; i < init_info.frames_in_flight; i++) {
		vkDestroySemaphore(owner->handle, image_available_semaphores[i], owner->owner->allocator);
		vkDestroySemaphore(owner->handle, render_finished_semaphores[i], owner->owner->allocator);
	}
	commands.clear();
	image_available_semaphores.clear();
	render_finished_semaphores.clear();
	available_present_modes.clear();
	available_surface_formats.clear();
 

	vkDestroySwapchainKHR(owner->handle, handle, owner->owner->allocator);

	if (init_info.destroy_surface) {
		vkDestroySurfaceKHR(owner->owner->handle, surface, owner->owner->allocator);
	}

	handle = VK_NULL_HANDLE;
}
} // namespace VB_NAMESPACE