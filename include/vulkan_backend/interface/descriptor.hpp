#ifndef VULKAN_BACKEND_DESCRIPTOR_HPP_
#define VULKAN_BACKEND_DESCRIPTOR_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <memory>
#elif defined(VB_DEV)
import std;
#endif

#include "../fwd.hpp"

VB_EXPORT
namespace VB_NAMESPACE {	
struct BindingInfo {
	u32 static constexpr kBindingAuto = ~0u;
	u32 static constexpr kMaxDescriptors = 8192;
	// Type of descriptor (e.g., DescriptorType::Sampler)
	DescriptorType const& type;
	// Binding number for this type.
	// If not specified, will be assigned automatically,
	// get it with GetBinding(type)
	u32 binding = kBindingAuto;
	// Max number of descriptors for this binding
	u32 count = kMaxDescriptors;
	// Shader stages that can access this resource
	ShaderStage stage_flags = ShaderStage::eAll;
	// Use update after bind flag
	bool update_after_bind = false;
	bool partially_bound   = true;
};

class Descriptor {
public:
	auto GetBinding(DescriptorType type) -> u32;
private:
	std::shared_ptr<DescriptorResource> resource;
	friend PipelineLibrary;
	friend DeviceResource;
	friend BufferResource;
	friend ImageResource;
	friend Device;
	friend Command;
};
} // namespace VB_NAMESPACE
#endif // VULKAN_BACKEND_DESCRIPTOR_HPP_