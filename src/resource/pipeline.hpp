#ifndef VULKAN_BACKEND_RESOURCE_PIPELINE_HPP_
#define VULKAN_BACKEND_RESOURCE_PIPELINE_HPP_

#ifndef VB_USE_STD_MODULE
#include <span>
#elif defined(VB_DEV)
import std;
#endif

#include <vulkan/vulkan.h>

#include "vulkan_backend/fwd.hpp"
#include "base.hpp"

namespace VB_NAMESPACE {
struct PipelineResource : ResourceBase<DeviceResource> {
	VkPipeline handle;
	VkPipelineLayout layout;
	PipelinePoint point;

	using ResourceBase::ResourceBase;

	void CreateLayout(std::span<VkDescriptorSetLayout const> descriptorLayouts);
	
	auto GetInstance() const -> InstanceResource* override;
	auto GetType() const -> char const* override {  return "PipelineResource"; }
	void Free() override;
};

} // namespace VB_NAMESPACE
#endif // VULKAN_BACKEND_RESOURCE_PIPELINE_HPP_