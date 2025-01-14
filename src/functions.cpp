#ifndef VB_USE_STD_MODULE
#include <memory>
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/functions.hpp"
#include "vulkan_backend/interface/instance.hpp"
#include "resource/instance.hpp"

namespace VB_NAMESPACE {
auto CreateInstance(InstanceInfo const& info) -> Instance {
	Instance instance;
	instance.resource = std::make_shared<InstanceResource>();
	instance.resource->Create(info);
	instance.resource->GetPhysicalDevices();
	return instance;
}
} // namespace VB_NAMESPACE