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

#include "vulkan_backend/interface/info/pipeline.hpp"
#include "vulkan_backend/util/algorithm.hpp"
#include "vulkan_backend/log.hpp"

VB_EXPORT
namespace VB_NAMESPACE {

struct VertexInputInfo;
struct PreRasterizationInfo;
struct FragmentShaderInfo;
struct FragmentOutputInfo;
struct VertexInputHash;
struct PreRasterizationHash;
struct FragmentOutputHash;
struct FragmentShaderHash;

// todo: use vectors instead of spans in key structs
struct VertexInputInfo {
	std::span<vk::Format const>						vertex_attributes;
	vk::PipelineInputAssemblyStateCreateInfo const& input_assembly;

	auto operator==(VertexInputInfo const& other) const -> bool {
		VB_LOG_TRACE("operator==(VertexInputInfo const& other)");
		return
		// vertex_attributes == other.vertex_attributes && 
		input_assembly == other.input_assembly;
	}
};

struct PreRasterizationInfo {
	vk::PipelineLayout									   layout = nullptr;
	std::span<vk::DynamicState const>					   dynamic_states;
	std::span<PipelineStage const>						   stages;
	std::optional<vk::PipelineTessellationStateCreateInfo> tessellation;
	vk::PipelineViewportStateCreateInfo					   viewport;
	vk::PipelineRasterizationStateCreateInfo			   rasterization;

	auto operator==(PreRasterizationInfo const& other) const -> bool {
		VB_LOG_TRACE("operator==(PreRasterizationInfo const& other)");
		return layout == other.layout && 
			// algo::IsSpanEqual(dynamic_states, other.dynamic_states) && 
			// algo::IsSpanEqual(stages, other.stages) && 
			tessellation == other.tessellation && 
			viewport == other.viewport && 
			rasterization == other.rasterization;
	}
};

struct FragmentShaderInfo {
	vk::PipelineLayout							   layout = nullptr;
	std::span<PipelineStage const>				   stages;
	vk::PipelineDepthStencilStateCreateInfo const& depth_stencil;
	// VkPipelineMultisampleStateCreateInfo if sample shading is enabled or renderpass is not VK_NULL_HANDLE

	bool operator==(FragmentShaderInfo const& other) const {
		VB_LOG_TRACE("operator==(FragmentShaderInfo const& other)");
		return layout == other.layout &&
			// stages == other.stages &&
			depth_stencil == other.depth_stencil;
	}
};

struct FragmentOutputInfo {
	std::span<vk::PipelineColorBlendAttachmentState const> blend_attachments;
	vk::PipelineColorBlendStateCreateInfo				   color_blend;
	vk::PipelineMultisampleStateCreateInfo				   multisample;
	std::span<vk::DynamicState const>					   dynamic_states;
	std::span<vk::Format const>							   color_formats;
	vk::Format											   depth_format;
	vk::Format											   stencil_format;

	auto operator==(FragmentOutputInfo const& other) const -> bool {
		VB_LOG_TRACE("operator==(FragmentOutputInfo const& other)");
		return
			// layout == other.layout &&
			// blend_attachments == other.blend_attachments &&
			color_blend == other.color_blend &&
			multisample == other.multisample &&
			dynamic_states == other.dynamic_states &&
			// color_formats == other.color_formats &&
			depth_format == other.depth_format &&
			stencil_format == other.stencil_format;
	}
};


// static inline auto operator==(PipelineStage const& lhs, PipelineStage const& rhs) -> bool {
// 	return lhs.stage == rhs.stage && lhs.source.data == rhs.source.data &&
// 		lhs.entry_point == rhs.entry_point && lhs.compile_options == rhs.compile_options;
// }

// auto VertexInputInfo::operator()(VertexInputInfo const& a, VertexInputInfo const& b) const -> bool {
// 	auto span_compare = [](auto const& a, auto const& b) -> bool {
// 		return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
// 	};
// 	return span_compare(a.vertexAttributes, b.vertexAttributes) && a.lineTopology == b.lineTopology;
// }

// auto FragmentOutputInfo::operator()(FragmentOutputInfo const& a, FragmentOutputInfo const& b) const -> bool {
// 	return std::equal(a.colorFormats.begin(), a.colorFormats.end(), b.colorFormats.begin()) && 
// 		a.layout == b.layout && 
// 		a.useDepth == b.useDepth && 
// 		a.depthFormat == b.depthFormat && 
// 		a.samples == b.samples;
// }

// auto FragmentShaderInfo::operator()(FragmentShaderInfo const& a, FragmentShaderInfo const& b) const -> bool {
// 	return a.stages.size() == b.stages.size() && 
// 		std::equal(a.stages.begin(), a.stages.end(), b.stages.begin()) && 
// 		a.layout == b.layout && 
// 		a.samples == b.samples;
// }
} // namespace VB_NAMESPACE
