#ifndef VB_USE_STD_MODULE
#include <limits>
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/interface/swapchain/swapchain.hpp"
#include "vulkan_backend/interface/device/device.hpp"
#include "vulkan_backend/interface/swapchain/swapchain.hpp"
#include "vulkan_backend/interface/instance/instance.hpp"
#include "vulkan_backend/interface/image/image.hpp"
#include "vulkan_backend/interface/command/command.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/interface/physical_device/physical_device.hpp"
#include "vulkan_backend/log.hpp"
#include "vulkan_backend/vk_result.hpp"

#include <vulkan/vulkan.h>

namespace VB_NAMESPACE {
Swapchain::Swapchain(Device& device, SwapchainInfo const& info)
	: ResourceBase(&device), info(info) {
	Create(info);
}

Swapchain::~Swapchain() { Free(); }

Swapchain::Swapchain(Swapchain&& other) noexcept
	: vk::SwapchainKHR(std::move(other)), ResourceBase(std::move(other)), info(std::move(other.info)) {
	// Transfer ownership of resources
	images = std::move(other.images);
	commands = std::move(other.commands);
	image_available_semaphores = std::move(other.image_available_semaphores);
	render_finished_semaphores = std::move(other.render_finished_semaphores);
	available_present_modes = std::move(other.available_present_modes);
	available_surface_formats = std::move(other.available_surface_formats);
	current_frame = other.current_frame;
	current_image_index = other.current_image_index;
	dirty = other.dirty;
}

Swapchain& Swapchain::operator=(Swapchain&& other) noexcept {
	if (this != &other) {
		Free(); // Free existing resources
		vk::SwapchainKHR::operator=(std::move(other));
		ResourceBase::operator=(std::move(other));
		info = std::move(other.info);
		images = std::move(other.images);
		commands = std::move(other.commands);
		image_available_semaphores = std::move(other.image_available_semaphores);
		render_finished_semaphores = std::move(other.render_finished_semaphores);
		available_present_modes = std::move(other.available_present_modes);
		available_surface_formats = std::move(other.available_surface_formats);
		current_frame = other.current_frame;
		current_image_index = other.current_image_index;
		dirty = other.dirty;
	}
	return *this;
}
bool Swapchain::AcquireNextImage() {
	// auto result = acquireNextImageKHR(
	// 	handle,
	// 	UINT64_MAX,
	// 	GetImageAvailableSemaphore(),
	// 	VK_NULL_HANDLE,
	// 	&current_image_index
	// );
	VB_VK_RESULT result;
	std::tie(result, current_image_index) = owner->acquireNextImage2KHR({
		.swapchain = *this,
		.timeout = std::numeric_limits<u64>::max(),
		.semaphore = GetImageAvailableSemaphore(),
		.fence = nullptr,
	});
	switch (result) {
	[[likely]]
	case vk::Result::eSuccess:
		return true;
	case vk::Result::eErrorOutOfDateKHR:
		VB_LOG_WARN("AcquireImage: Out of date");
		dirty = true;
		return false;
	case vk::Result::eSuboptimalKHR:
		return false;
	default:
		VB_CHECK_VK_RESULT(result, "Failed to acquire swap chain image!");
		return false;
	}
}

// EndCommandBuffer + vkQueuePresentKHR
void Swapchain::SubmitAndPresent(vk::Queue const& submit, vk::Queue const& present) {
	auto& cmd = GetCommandBuffer();

	cmd.End();
	cmd.QueueSubmit( submit, {
		.waitSemaphoreInfos   = {{{.semaphore = GetImageAvailableSemaphore()}}},
		.signalSemaphoreInfos = {{{.semaphore = GetRenderFinishedSemaphore()}}}
	});

	vk::Semaphore present_wait = GetRenderFinishedSemaphore();

	VB_VK_RESULT result = present.presentKHR({
		.waitSemaphoreCount = 1, // TODO(nm): pass with info
		.pWaitSemaphores = &present_wait,
		.swapchainCount = 1,
		.pSwapchains = this,
		.pImageIndices = &current_image_index,
		.pResults = nullptr,
	});

	switch (result) {
	case vk::Result::eSuccess: break;
	case vk::Result::eErrorOutOfDateKHR:
	case vk::Result::eSuboptimalKHR:
		VB_LOG_WARN("vkQueuePresentKHR: %s", StringFromVkResult(result));
		dirty = true;
		return;
	default:
		VB_CHECK_VK_RESULT(result, "Failed to present swap chain image!"); 
		break;
	}

	current_frame = (current_frame + 1) % info.frames_in_flight;

}

void Swapchain::Recreate(u32 width, u32 height) {
	VB_ASSERT(width > 0 && height > 0, "Window size is 0, swapchain NOT to be recreated");
	VB_VK_RESULT result;
	for (auto& cmd: commands) {
		result = owner->waitForFences(1, &cmd.fence, vk::True, std::numeric_limits<u32>::max());
		VB_CHECK_VK_RESULT(result, "Failed to wait for fence!");
	}

	for (auto& image : images) {
		owner->destroyImageView(image.view, owner->GetAllocator());
	}
	images.clear();
	CreateSurfaceFormats();
	info.extent = ChooseExtent({width, height});
	auto [format, colorSpace] = ChooseSurfaceFormat({
		info.preferred_format, 
		info.color_space
	});
	info.preferred_format = format;
	info.color_space = colorSpace;
	info.present_mode = ChoosePresentMode(info.present_mode);
	CreateSwapchain();
	CreateImages();
}

auto Swapchain::GetCurrentImage() -> Image& {
	return images[current_image_index];
}

auto Swapchain::GetCommandBuffer() -> Command& {
	return commands[current_frame];
}

bool Swapchain::GetDirty() {
	return dirty;
}

auto Swapchain::GetFormat() -> vk::Format {
	return info.preferred_format;
}
auto Swapchain::GetExtent() const -> vk::Extent2D {
	return info.extent;
}

bool Swapchain::SupportsFormat(vk::Format format, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
	vk::FormatProperties props;
	owner->physical_device->getFormatProperties(format, &props);

	if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
		return true;
	} else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
		return true;
	}

	return false;
}

auto Swapchain::ChoosePresentMode(vk::PresentModeKHR const& desired_present_mode) -> vk::PresentModeKHR {
	for (auto const& mode : available_present_modes) {
		if (mode == static_cast<vk::PresentModeKHR>(desired_present_mode)) {
			return desired_present_mode;
		}
	}
	VB_LOG_WARN("Preferred present mode not available!");
	// FIFO is guaranteed to be available
	return vk::PresentModeKHR::eFifo;
}

auto Swapchain::ChooseExtent(vk::Extent2D const& desired_extent) -> vk::Extent2D {
	if (this->surface_capabilities.currentExtent.width != std::numeric_limits<u32>::max()) {
		return {
			surface_capabilities.currentExtent.width,
			surface_capabilities.currentExtent.height,
		};
	}
	return vk::Extent2D {
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

void Swapchain::CreateSurfaceFormats() {
	// get capabilities
	VB_VK_RESULT result;
	result = owner->physical_device->getSurfaceCapabilitiesKHR(info.surface, &surface_capabilities);

	// get surface formats
	u32 formatCount;
	result = owner->physical_device->getSurfaceFormatsKHR(info.surface, &formatCount, nullptr);
	if (formatCount != 0) {
		available_surface_formats.clear();
		available_surface_formats.resize(formatCount);
		result = owner->physical_device->getSurfaceFormatsKHR(info.surface, &formatCount, available_surface_formats.data());
	}

	// get present modes
	u32 modeCount;
	result = owner->physical_device->getSurfacePresentModesKHR(info.surface, &modeCount, nullptr);
	if (modeCount != 0) {
		available_present_modes.clear();
		available_present_modes.resize(modeCount);
		result = owner->physical_device->getSurfacePresentModesKHR(info.surface, &modeCount, available_present_modes.data());
	}

	// set this device as not suitable if no surface formats or present modes available
	bool suitable = (modeCount > 0);
	suitable &= (formatCount > 0);
	VB_ASSERT(suitable, "Selected device invalidated after surface update.");
}

auto Swapchain::ChooseSurfaceFormat(vk::SurfaceFormatKHR const& desired_surface_format) -> vk::SurfaceFormatKHR {
	for (auto const& availableFormat : available_surface_formats) {
		if (availableFormat.format == desired_surface_format.format &&
			availableFormat.colorSpace == desired_surface_format.colorSpace) {
			return {availableFormat.format, availableFormat.colorSpace};
		}
	}
	VB_LOG_WARN("Preferred surface format not available!");
	return {available_surface_formats[0].format, available_surface_formats[0].colorSpace};
}

void Swapchain::CreateSwapchain() {
	auto const& capabilities = surface_capabilities;

	// acquire additional images to prevent waiting for driver's internal operations
	u32 image_count = info.frames_in_flight + info.additional_images;

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
	vk::SwapchainCreateInfoKHR createInfo {
		.surface = info.surface,
		.minImageCount = image_count,
		.imageFormat = vk::Format(info.preferred_format),
		.imageColorSpace = vk::ColorSpaceKHR(info.color_space),
		.imageExtent = {info.extent.width, info.extent.height},
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment // if we want to render to a separate image first to perform post-processing
						| vk::ImageUsageFlagBits::eTransferDst, // we should add this image usage
		.imageSharingMode = vk::SharingMode::eExclusive, // don't support different graphics and present family
		.queueFamilyIndexCount = 0, // only when imageSharingMode is VK_SHARING_MODE_CONCURRENT
		.pQueueFamilyIndices = nullptr, // only when imageSharingMode is VK_SHARING_MODE_CONCURRENT
		.preTransform = capabilities.currentTransform, // blend the images with other windows in the window system
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = vk::PresentModeKHR(info.present_mode),
		.clipped = true, // clip pixels behind our window
		.oldSwapchain = *this,
	};

	VB_VK_RESULT result = owner->createSwapchainKHR(&createInfo, owner->GetAllocator(), this);
	VB_CHECK_VK_RESULT(result, "Failed to create swap chain!");	
	owner->destroySwapchainKHR(createInfo.oldSwapchain, owner->GetAllocator());
}

void Swapchain::CreateImages() {
	u32 imageCount;
	VB_VK_RESULT result;
	result = owner->getSwapchainImagesKHR(*this, &imageCount, nullptr);
	VB_VLA(vk::Image, vk_images, imageCount);
	result = owner->getSwapchainImagesKHR(*this, &imageCount, vk_images.data());

	// Create image views
	images.clear();
	images.reserve(vk_images.size());
	for (auto vk_image: vk_images) {
		// Create image name
		VB_FORMAT_WITH_SIZE(image_name, 512, "SwapChain Image %zu", images.size());
		images.push_back(Image(vk_image, vk::ImageView{}, {info.extent.width, info.extent.height, 1}, image_name));
		
		// Create image view
		vk::ImageViewCreateInfo viewInfo{
			.image = vk_image,
			.viewType = vk::ImageViewType::e2D,
			.format = vk::Format(info.preferred_format),
			.subresourceRange {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};
		VB_VK_RESULT result = owner->createImageView(&viewInfo, owner->GetAllocator(), &images.back().view);
		VB_CHECK_VK_RESULT(result, "Failed to create SwapChain image view!");

		// Add debug names
		if (owner->GetInstance().IsDebugUtilsEnabled()) {
			images.back().SetDebugUtilsName(image_name);
			VB_FORMAT_WITH_SIZE(view_name, 512, "%s%s", image_name, "View");
			images.back().SetDebugUtilsViewName(image_name);
		}
	}
}

void Swapchain::CreateSemaphores() {
	image_available_semaphores.resize(info.frames_in_flight);
	render_finished_semaphores.resize(info.frames_in_flight);

	vk::SemaphoreCreateInfo semaphoreInfo{};

	for (size_t i = 0; i < info.frames_in_flight; ++i) {
		VB_VK_RESULT
		result = owner->createSemaphore(&semaphoreInfo, owner->GetAllocator(), &image_available_semaphores[i]);
		VB_CHECK_VK_RESULT(result, "Failed to create semaphore!");
		result = owner->createSemaphore(&semaphoreInfo, owner->GetAllocator(), &render_finished_semaphores[i]);
		VB_CHECK_VK_RESULT(result, "Failed to create semaphore!");
	}
}

void Swapchain::CreateCommands(u32 queueFamilyindex) {
	for (u32 i = 0; i < info.frames_in_flight; ++i) {
		commands.emplace_back(*owner, queueFamilyindex);
	}
}

void Swapchain::Create(SwapchainInfo const& init_info) {
	// this is a necessary hackery
	this->info = init_info;
	CreateSurfaceFormats();
	auto [format, colorSpace] = ChooseSurfaceFormat({
		info.preferred_format,
		info.color_space
	});
	info.preferred_format = format;
	info.color_space = colorSpace;
	info.present_mode = ChoosePresentMode(info.present_mode);
	info.extent = ChooseExtent(info.extent);
	CreateSwapchain();
	CreateImages();
	CreateSemaphores();
	CreateCommands(info.queueFamilyindex);
	current_frame = 0;
	current_image_index = 0;
	dirty = false;
	VB_LOG_TRACE("Created Swapchain");
}

auto Swapchain::GetResourceTypeName() const -> char const* {
	return "SwapChainResource";
}

void Swapchain::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), GetName().data());
	VB_VK_RESULT result;
	for (auto& cmd: commands) {
		result = owner->waitForFences(1, &cmd.fence, vk::True, std::numeric_limits<u32>::max());
		VB_CHECK_VK_RESULT(result, "Failed to wait for fence!");
	}

	for (auto& image : images) {
		owner->destroyImageView(image.view, owner->GetAllocator());
	}
	images.clear();

	for (size_t i = 0; i < info.frames_in_flight; i++) {
		owner->destroySemaphore(image_available_semaphores[i], owner->GetAllocator());
		owner->destroySemaphore(render_finished_semaphores[i], owner->GetAllocator());
	}
	commands.clear();
	image_available_semaphores.clear();
	render_finished_semaphores.clear();
	available_present_modes.clear();
	available_surface_formats.clear();
 
	owner->destroySwapchainKHR(*this, owner->GetAllocator());

	if (info.destroy_surface) {
		owner->GetInstance().destroySurfaceKHR(info.surface, owner->GetAllocator());
	}
	vk::SwapchainKHR::operator=(nullptr);
	info.surface = nullptr;
}
} // namespace VB_NAMESPACE