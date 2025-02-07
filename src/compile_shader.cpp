#ifndef VB_USE_STD_MODULE
#include <filesystem>
#include <fstream>
#include <string_view>
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/compile_shader.hpp"
#include "vulkan_backend/util/algorithm.hpp"
#include "vulkan_backend/util/hash_functions.hpp"
#include "vulkan_backend/log.hpp"


#if !defined( VB_MAX_COMPILE_STRING_SIZE )
#define VB_MAX_COMPILE_STRING_SIZE 1024
#endif

#if !defined( VB_MAX_PATH_SIZE )
#define VB_MAX_PATH_SIZE 256
#endif

namespace VB_NAMESPACE {
auto ReadBinaryFile(std::string_view const& path) -> std::vector<char> {
	std::ifstream file(path.data(), std::ios::ate | std::ios::binary);
	VB_ASSERT(file.is_open(), "Failed to open file!");
	std::size_t file_size = static_cast<std::size_t>(file.tellg());
	std::vector<char> buffer(file_size);
	file.seekg(0);
	file.read(buffer.data(), file_size);
	file.close();
	return buffer;
}

void CompileShader(PipelineStage const& stage, char const* out_file) {
	char compile_string[VB_MAX_COMPILE_STRING_SIZE];
	char const* entry = "";
	char const* target = "";
	char const* options = "";
	if (stage.compiler == "slangc") {
		entry = "-entry ";
	} else {
		if (stage.compiler == "glslangValidator") {
			entry	= "-e ";
			options = "-V";
		} else if (stage.compiler == "glslc") {
			entry = "-fentry-point=";
		}
	}

	std::snprintf(compile_string, VB_MAX_COMPILE_STRING_SIZE - 1, "%s %s %s -o %s %s%s %s",
		stage.compiler.data(), options, stage.source.data.data(), out_file, entry,
			stage.entry_point.data(), stage.compile_options.data());

	VB_LOG_TRACE("[ ShaderCompiler ] Command: %s", compile_string);
	while(std::system(compile_string)) {
		VB_LOG_ERROR("[ ShaderCompiler ] Error! Press something to Compile Again");
		std::getchar();
	}
}

// template <typename T>
// void Replace(std::span<T> buffer, T const& old_value, T const& new_value) {
// 	std::replace(buffer.begin(), buffer.end(), old_value, new_value);
// }

auto LoadShader(PipelineStage const& stage) -> std::vector<char> {
	char out_file_name[VB_MAX_PATH_SIZE + 1];
	char out_file[VB_MAX_PATH_SIZE + 1];
	char const* file_name_input = stage.source.data.data();
	VB_ASSERT(stage.source.type == Source::Type::File || stage.source.type == Source::Type::FileSpirV, "LoadShader: source type not implemented");
	switch (stage.source.type) {
	case Source::Type::File: {
		int size = std::snprintf(out_file_name, VB_MAX_PATH_SIZE, "%s", stage.source.data.data());
		std::span file_name_view(out_file_name, size);
		std::replace(file_name_view.begin(), file_name_view.end(), '\\', '-');
		std::replace(file_name_view.begin(), file_name_view.end(), '/', '-');
		// std::ranges::replace(file_name_view, '.', '-');
	} break;
	case Source::Type::FileSpirV:
		return ReadBinaryFile(stage.source.data);

	default: {
		u64 hash = HashFnv1a64(stage.source.data);
		std::snprintf(out_file_name, VB_MAX_PATH_SIZE, "%s-%zx", stage.entry_point.data(), hash);
	}
	}


	if (stage.out_path.empty()) {
		VB_FORMAT_TO_BUFFER(out_file, "./%s.spv", out_file_name);
	} else {
		VB_FORMAT_TO_BUFFER(out_file, "%s/%s.spv", stage.out_path.data(), out_file_name);
	}
	// std::printf("Output file name: %s\n", out_file_name);
	// std::printf("Output file: %s\n", out_file);

	std::filesystem::path out_file_path = out_file;
	out_file_path = out_file_path.parent_path();
	if (!std::filesystem::exists(out_file_path)) {
		std::filesystem::create_directories(out_file_path);
	}
	bool compilation_required =
		(stage.flags & PipelineStage::Flags::kAllowSkipCompilation) != PipelineStage::Flags::kAllowSkipCompilation;

	
	// check if already compiled to .spv
	if (stage.source.type == Source::Type::File) [[likely]] {
		std::filesystem::path const shader_path = stage.source.data;
		if (std::filesystem::exists(out_file) && std::filesystem::exists(shader_path)) {
			auto lastSpvUpdate	  = std::filesystem::last_write_time(out_file);
			auto lastShaderUpdate = std::filesystem::last_write_time(shader_path);
			if (lastShaderUpdate > lastSpvUpdate) {
				compilation_required = true;
			}
		} else {
			compilation_required = true;
		}
	} else {
		if (!std::filesystem::exists(out_file)) {
			compilation_required = true;
		}
	}
	
	if (compilation_required) {
		CompileShader(stage, out_file);
		return ReadBinaryFile(out_file);
	} else {
		return ReadBinaryFile(out_file);
	}
}
} // namespace VB_NAMESPACE