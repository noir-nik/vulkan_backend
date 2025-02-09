#pragma once

#ifndef VB_USE_STD_MODULE
#include <span>
#include <string_view>

#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/classes/base.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/pipeline_layout/info.hpp"

VB_EXPORT
namespace VB_NAMESPACE {

// Class for garbage-collecting vk::PipelineLayout
class PipelineLayout : public vk::PipelineLayout, public ResourceBase<Device> {
  public:
	// No-op constructor
	PipelineLayout() = default;

	// Constructor from already-created raw vk::PipelineLayout for garbage-collecting
	PipelineLayout(Device& device, vk::PipelineLayout pipeline_layout);

	// RAII constructor, calls Create
	PipelineLayout(Device& device, PipelineLayoutInfo const& info);

	// Move-constructor
	PipelineLayout(PipelineLayout&& other);

	// Move-assignment
	PipelineLayout& operator=(PipelineLayout&& other);

	// Destructor, calls Free
	~PipelineLayout();

	// Create with result checked
	auto Create(Device& device, PipelineLayoutInfo const& info) -> vk::Result;

	// Frees layout
	void Free() override;

	auto GetDevice() const -> Device& { return *GetOwner(); }
	auto GetResourceTypeName() const -> char const* override;

  private:
};

} // namespace VB_NAMESPACE
