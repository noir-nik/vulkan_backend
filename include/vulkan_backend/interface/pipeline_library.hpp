#pragma once

#ifndef VB_USE_STD_MODULE
#include <span>
#include <unordered_map>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/classes/no_copy_no_move.hpp"
#include "vulkan_backend/interface/info/pipeline_library.hpp"
#include "vulkan_backend/interface/pipeline.hpp"
#include "vulkan_backend/classes/base.hpp"
#include "vulkan_backend/util/hash.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
class PipelineLibrary : NoCopyNoMove, ResourceBase<Device> {
public:
	auto CreatePipeline(GraphicsPipelineInfo const& info) -> Pipeline;
	void Free();

private:
	auto CreateVertexInputInterface(VertexInputInfo const& info) -> vk::Pipeline;
	auto CreatePreRasterizationShaders(PreRasterizationInfo const& info) -> vk::Pipeline;
	auto CreateFragmentShader(FragmentShaderInfo const& info) -> vk::Pipeline;
	auto CreateFragmentOutputInterface(FragmentOutputInfo const& info) -> vk::Pipeline;
	auto LinkPipeline(std::array<vk::Pipeline, 4> const& pipelines, vk::PipelineLayout layout, bool link_time_optimization) -> vk::Pipeline;

	Device* device = nullptr;
	std::unordered_map<VertexInputInfo, vk::Pipeline> vertex_input_interfaces;
	std::unordered_map<PreRasterizationInfo, vk::Pipeline> pre_rasterization_shaders;
	std::unordered_map<FragmentOutputInfo, vk::Pipeline> fragment_output_interfaces;
	std::unordered_map<FragmentShaderInfo, vk::Pipeline> fragment_shaders;
	friend Device;
};
} // namespace VB_NAMESPACE
