#pragma once

#ifndef VB_USE_STD_MODULE
#include <span>
#include <string_view>

#elif defined(VB_DEV)
import std;
#endif

#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/pipeline/info.hpp"
#include "vulkan_backend/types.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
void CreateShaderModulesAndStagesInfos(Device const* device, std::span<const PipelineStage> stages,
								vk::ShaderModule*				  p_shader_modules,
								vk::PipelineShaderStageCreateInfo* p_shader_stages);
								
void CreateShaderModuleInfos(std::span<const PipelineStage> stages, std::vector<char>* p_bytes,
									vk::ShaderModuleCreateInfo*		   p_shader_module_create_infos,
									vk::PipelineShaderStageCreateInfo* p_shader_stages);

void CreateVertexDescriptionsFromAttributes(std::span<vk::Format const>			vertex_attributes,
													vk::VertexInputAttributeDescription* p_attribute_descs,
													u32*									p_stride);
} // namespace VB_NAMESPACE