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
#include "vulkan_backend/log.hpp"

namespace VB_NAMESPACE {
auto ReadBinaryFile(std::string_view const& path) -> std::vector<char> {
	std::ifstream file(path.data(), std::ios::ate | std::ios::binary);
	VB_ASSERT(file.is_open(), "Failed to open file!");
	std::size_t fileSize = static_cast<std::size_t>(file.tellg());
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();
	return buffer;
}

void CompileShader(PipelineStage const& stage, char const* out_file) {
	char compile_string[VB_MAX_COMPILE_STRING_SIZE];

	if (stage.source.data.ends_with(".slang")){
		std::snprintf(compile_string, VB_MAX_PATH_SIZE, "%s %s -o %s -entry %s %s",
			stage.compiler.data(), stage.source.data.data(), out_file,
				stage.entry_point.data(), stage.compile_options.data());
	} else {
		std::snprintf(compile_string, VB_MAX_PATH_SIZE, "%s -gVS -V %s -o %s -e %s %s", 
			stage.compiler.data(), stage.source.data.data(), out_file,
				stage.entry_point.data(), stage.compile_options.data());
	}

	VB_LOG_TRACE("[ ShaderCompiler ] Command: %s", compile_string);
	while(std::system(compile_string)) {
		VB_LOG_ERROR("[ ShaderCompiler ] Error! Press something to Compile Again");
		std::getchar();
	}
}
template <typename T>
void Replace(std::span<T> buffer, T const& old_value, T const& new_value) {
	std::replace(buffer.begin(), buffer.end(), old_value, new_value);
}
auto LoadShader(PipelineStage const& stage) -> std::vector<char> {
	char out_file_name[VB_MAX_PATH_SIZE + 1];
	char out_file[VB_MAX_PATH_SIZE + 1];

	int size = std::snprintf(out_file_name, VB_MAX_PATH_SIZE, "%s", stage.source.data.data());
	std::span file_name_view(out_file_name, size);
	std::replace(file_name_view.begin(), file_name_view.end(), '\\', '-');
	std::replace(file_name_view.begin(), file_name_view.end(), '/', '-');
	// std::ranges::replace(file_name_view, '.', '-');

	if (stage.out_path.empty()) {
		VB_FORMAT_TO_BUFFER(out_file, "./%s.spv", file_name_view.data());
	} else {
		VB_FORMAT_TO_BUFFER(out_file, "%s/%s.spv", stage.out_path.data(), stage.source.data.data());
	}
	std::filesystem::path const shader_path = stage.source.data;
	std::filesystem::path out_file_path = out_file;
	out_file_path = out_file_path.parent_path();
	bool compilation_required =
		(stage.flags & PipelineStage::Flags::kAllowSkipCompilation) != PipelineStage::Flags::kAllowSkipCompilation;

	if (!std::filesystem::exists(out_file_path)){
		std::filesystem::create_directories(out_file_path);
	}
	// check if already compiled to .spv
	if (std::filesystem::exists(out_file) && std::filesystem::exists(shader_path)) {
		auto lastSpvUpdate = std::filesystem::last_write_time(out_file);
		auto lastShaderUpdate = std::filesystem::last_write_time(shader_path);
		if (lastShaderUpdate > lastSpvUpdate) {
			compilation_required = true;
		}
	} else {
		compilation_required = true;
	}
	if (compilation_required) {
		CompileShader(stage, out_file);
		return ReadBinaryFile(out_file);
	} else {
		return ReadBinaryFile(out_file);
	}
}
} // namespace VB_NAMESPACE