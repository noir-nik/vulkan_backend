#ifndef VULKAN_BACKEND_PHYSICAL_DEVICE_HPP_
#define VULKAN_BACKEND_PHYSICAL_DEVICE_HPP_

#ifndef VB_USE_STD_MODULE
#include <memory>
#elif defined(VB_DEV)
import std;
#endif

#include "../fwd.hpp"

VB_EXPORT
namespace VB_NAMESPACE {	
class PhysicalDevice{
	std::shared_ptr<PhysicalDeviceResource> resource;
	friend PhysicalDeviceResource;
	friend DeviceResource;
	friend Device;
};
} // namespace VB_NAMESPACE
#endif // VULKAN_BACKEND_PHYSICAL_DEVICE_HPP_