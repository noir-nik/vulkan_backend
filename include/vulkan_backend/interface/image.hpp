#ifndef VULKAN_BACKEND_IMAGE_HPP_
#define VULKAN_BACKEND_IMAGE_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <memory>
#include <string_view>
#elif defined(VB_DEV)
import std;
#endif

#include "../structs.hpp"
#include "../fwd.hpp"

VB_EXPORT
namespace VB_NAMESPACE {	
struct SamplerInfo {
	Filter magFilter = Filter::eLinear;
	Filter minFilter = Filter::eLinear;
	MipmapMode mipmapMode = MipmapMode::eLinear;
	struct Wrap {
		WrapMode u = WrapMode::eRepeat;
		WrapMode v = WrapMode::eRepeat;
		WrapMode w = WrapMode::eRepeat;
	} wrap = {};
	float mipLodBias = 0.0f;
	bool anisotropyEnable = false;
	float maxAnisotropy = 0.0f;
	bool compareEnable = false;
	CompareOp compareOp = CompareOp::eAlways;
	float minLod = 0.0f;
	float maxLod = 1.0f;
	BorderColor borderColor = BorderColor::eIntOpaqueBlack;
	bool unnormalizedCoordinates = false;
};
	
struct ImageInfo {
	Extent3D const&          extent;
	Format const&            format;
	ImageUsageFlags const&   usage;
	SampleCount              samples = SampleCount::e1;
	SamplerInfo              sampler = {};
	u32                      layers  = 1;
	std::string_view         name    = "";
};

class Image {
	std::shared_ptr<ImageResource> resource;
	friend SwapChainResource;
	friend Swapchain;
	friend DeviceResource;
	friend Command;
public:
	auto GetResourceID() const -> u32;
	auto GetFormat()     const -> Format;
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_IMAGE_HPP_