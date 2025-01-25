#pragma once

#ifndef VB_USE_STD_MODULE
#include <span>
#include <string_view>

#elif defined(VB_DEV)
import std;
#endif

#include "vulkan_backend/classes/base.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/pipeline/info.hpp"
#include "vulkan_backend/types.hpp"

VB_EXPORT
namespace VB_NAMESPACE {

class Pipeline : public vk::Pipeline, public Named, public ResourceBase<Device> {
  public:
	// Constructor from already-created raw vk::Pipeline and vk::PipelineLayout.
	// It will destroy vk::Pipeline pipeline on destruction.
	// To create a new pipeline, use Device::CreatePipeline() factory function.
	Pipeline(Device& device, vk::Pipeline pipeline, vk::PipelineLayout layout, vk::PipelineBindPoint point,
			 std::string_view name = "");

	Pipeline(Device& device, PipelineInfo const& info);
	Pipeline(Device& device, GraphicsPipelineInfo const& info);

	Pipeline(Pipeline&& other);
	Pipeline& operator=(Pipeline&& other);

	~Pipeline();

  private:
	void Create(PipelineInfo const& info);
	void Create(GraphicsPipelineInfo const& info);
	auto GetResourceTypeName() const -> char const* override;
	void Free() override;



	vk::PipelineLayout	  layout;
	vk::PipelineBindPoint point;
	friend Device;
	friend PipelineLibrary;
	friend Command;
};

} // namespace VB_NAMESPACE
