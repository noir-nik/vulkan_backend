#pragma once

#ifndef VB_USE_STD_MODULE
#include <string_view>
#include <optional>
#include <span>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/defaults/pipeline.hpp"

VB_EXPORT
namespace VB_NAMESPACE {

struct Source  {
	enum Type {
		File,
		FileSpirV,
		RawGlsl,
		RawSlang,
		RawSpirV,
	};
	// file or string with shader code
	std::string_view data;
	Type type = File;
	bool operator==(Source const& other) const { return data == other.data && type == other.type; }
};

struct PipelineStage {
	enum class Flags {
		kNone = 0,
		// Option to not recompile the shader if respective 'filename'.spv exists and is up-to-date.
		// But be careful with this, because if 'compile_options' such as define macros are changed,
		// this might use old .spv file and cause hard to find bugs
		kAllowSkipCompilation = 1 << 1,
		// Set link time optimization flag when using graphics pipeline library
		// Note: to perform link time optimizations, the 'link_time_optimization' MUST be true
		// for all pipeline libraries (stages) that are being linked together.
		kLinkTimeOptimization = 1 << 2,
	};

	// Shader stage, e.g. Vertex, Fragment, Compute or other
	vk::ShaderStageFlagBits stage;

	// Shader code: any glsl or .slang or .spv. It can be file name or raw code.
	// Specify type if data is a string with shader code
	Source source;

	// Output path for compiled .spv. Has effect only if compilation is done
	std::string_view out_path = ".";

	// Shader entry point
	std::string_view entry_point = "main";
	
	// Shader compiler
	std::string_view compiler = "glslc";

	// Additional compiler options
	std::string_view compile_options = "";

	// Flags
	vk::Flags<PipelineStage::Flags> flags = PipelineStage::Flags::kLinkTimeOptimization;

	// Specialization constants
	vk::SpecializationInfo const& specialization_info = {};
	// SpecializationInfo specialization_info;

	bool operator==(PipelineStage const& other) const {
		return stage == other.stage && source == other.source &&
			   entry_point == other.entry_point && compile_options == other.compile_options &&
			   specialization_info == other.specialization_info;
	}
};

// Necessary for any pipeline creation
struct PipelineInfo {
	std::span<PipelineStage const> stages;
	vk::PipelineLayout			   layout;
	std::string_view			   name	  = "";
};

// Data for graphics pipeline creation
struct GraphicsPipelineInfo : PipelineInfo {
	std::span<vk::Format const>							              vertex_attributes;
	vk::PipelineInputAssemblyStateCreateInfo const&		              input_assembly	 = defaults::kInputAssembly;
	std::optional<vk::PipelineTessellationStateCreateInfo> const&     tessellation		 = {}; // == nullptr if empty
	vk::PipelineViewportStateCreateInfo const&			              viewport			 = defaults::kOneDynamicViewportOneDynamicScissor;
	vk::PipelineRasterizationStateCreateInfo const&		              rasterization	     = defaults::kRasterization;
	vk::PipelineMultisampleStateCreateInfo const&		              multisample		 = defaults::kMultisampleOneSample;
	vk::PipelineDepthStencilStateCreateInfo const&		              depth_stencil	     = defaults::kNoDepthNoStencil; // Other defaults are available
	// if blend_attachments.size() < color_formats.size(), the rest will be filled with defaults::kBlendAttachment
	std::span<vk::PipelineColorBlendAttachmentState const>            blend_attachments  = defaults::kBlendAttachments;
	// .attachmentCount and .pAttachments will be set automatically
	vk::PipelineColorBlendStateCreateInfo const&                      color_blend		 = defaults::kBlend;
	std::span<vk::DynamicState const>					              dynamic_states	 = defaults::kDynamicStates;
	//vk::PipelineRenderingCreateInfo is filled from these:           
	std::span<vk::Format const>							              color_formats	     = {};
	vk::Format											              depth_format		 = vk::Format::eUndefined;
	vk::Format											              stencil_format	 = vk::Format::eUndefined;
};

} // namespace VB_NAMESPACE
