#ifndef VULKAN_BACKEND_COMPILE_SHADER_HPP_
#define VULKAN_BACKEND_COMPILE_SHADER_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <string_view>
#else
import std;
#endif

#include "vulkan_backend/config.hpp"
#include "vulkan_backend/interface/pipeline.hpp"

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
 * @return A vector of characters containing the compiled shader.
 */
auto CompileShader(Pipeline::Stage const& stage, char const* out_file) -> std::vector<char>;

/**
 * @brief Loads a shader stage from a file, compile if needed.
 * Use this function in most cases.
 * @param[in] stage The shader stage to load.
 * @return A vector of characters containing the loaded shader.
 */
auto LoadShader(Pipeline::Stage const& stage) -> std::vector<char>;
} // namespace VB_NAMESPACE
#endif // VULKAN_BACKEND_COMPILE_SHADER_HPP_