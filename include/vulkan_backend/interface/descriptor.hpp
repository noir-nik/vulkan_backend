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
class Descriptor {
public:
	auto GetBinding(DescriptorType type) -> u32;
private:
	std::shared_ptr<DescriptorResource> resource;
	friend DeviceResource;
	friend BufferResource;
	friend ImageResource;
	friend Device;
	friend Command;
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_DESCRIPTOR_HPP_