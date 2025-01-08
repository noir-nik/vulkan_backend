#ifndef VULKAN_BACKEND_PIPELINE_HPP_
#define VULKAN_BACKEND_PIPELINE_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <memory>
#include <string_view>
#include <span>
#elif defined(VB_DEV)
import std;
#endif

#include "../fwd.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
struct Source  {
	enum Type {
		File,
		RawGlsl,
		RawSlang,
		RawSpirv,
	};
	std::string_view const& data;
	Type type = File;
};

class Pipeline {
public:
	struct Stage {
		// Shader stage, e.g. Vertex, Fragment, Compute or other
		ShaderStage const& stage;

		// Shader code: any glsl or .slang or .spv. It can be file name or raw code.
		// Specify type if data is a string with shader code
		Source const& source;

		// Output path for compiled .spv. Has effect only if compilation is done
		std::string_view out_path = ".";

		// Shader entry point
		std::string_view entry_point = "main";
		
		// Shader compiler
		std::string_view compiler = "glslangValidator";

		// Additional compiler options
		std::string_view compile_options = "";

		// Option to not recompile the shader if respective 'filename'.spv exists and is up-to-date.
		// But be careful with this, because if 'compile_options' such as define macros are changed,
		// this might use old .spv file and cause hard to find bugs
		bool allow_skip_compilation = false;

	};
private:
	std::shared_ptr<PipelineResource> resource;
	friend DeviceResource;
	friend PipelineLibrary;
	friend Command;
};

struct PipelineInfo {
	// Necessary for any pipeline creation
	PipelinePoint const&                    point;
	std::span<Pipeline::Stage const> const& stages;
	
	// Data for graphics pipeline
	std::span<Format const>          vertexAttributes  = {};
	std::span<Format const>          color_formats     = {};
	bool                             use_depth         = false;
	Format                           depth_format      = Format::eUndefined;
	SampleCount                      samples           = SampleCount::e1;
	CullModeFlags                    cullMode          = CullMode::eNone;
	bool                             line_topology     = false;
	std::span<DynamicState const>    dynamicStates     = {{DynamicState::eViewport, DynamicState::eScissor}};
	std::string_view                 name              = "";
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_PIPELINE_HPP_