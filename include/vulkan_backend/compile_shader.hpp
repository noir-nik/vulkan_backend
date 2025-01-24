#pragma once

#ifndef VB_USE_STD_MODULE
#include <string_view>
#elif defined(VB_DEV)
import std;
#endif

#include "vulkan_backend/config.hpp"
#include "vulkan_backend/interface/info/pipeline.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
/**
 * @brief Reads a binary file into a vector of characters.
 * @param[in] path The path to the file to read.
 * @return A vector of characters containing the contents of the file.
 */
auto ReadBinaryFile(std::string_view const& path) -> std::vector<char>;

/**
 * @brief Compiles a shader stage using the shader compiler provided by the stage.
 * @param[in] stage The shader stage to compile.
 * @param[out] out_file The path to the output file.
 */
void CompileShader(PipelineStage const& stage, char const* out_file);

/**
 * @brief Loads a shader stage from a file, compile if needed.
 * Use this function in most cases.
 * @param[in] stage The shader stage to load.
 * @return A vector of characters containing the loaded shader.
 */
auto LoadShader(PipelineStage const& stage) -> std::vector<char>;
} // namespace VB_NAMESPACE
