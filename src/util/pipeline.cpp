#ifndef VB_USE_STD_MODULE
#include <string_view>
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/compile_shader.hpp"
#include "vulkan_backend/interface/device/device.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/util/enumerate.hpp"
#include "vulkan_backend/util/pipeline.hpp"
#include "vulkan_backend/vk_result.hpp"

namespace VB_NAMESPACE {
void CreateShaderModulesAndStagesInfos(Device const& device, std::span<const PipelineStage> stages,
						vk::ShaderModule*				   p_shader_modules,
						vk::PipelineShaderStageCreateInfo* p_shader_stages) {
	for (auto [i, stage] : util::enumerate(stages)) {
		// Load or compile shader
		std::vector<char> bytes = LoadShader(stage);

		// Create shader module
		vk::ShaderModuleCreateInfo createInfo{
			.codeSize = bytes.size(),
			.pCode	  = reinterpret_cast<const u32*>(bytes.data()),
		};
		VB_VK_RESULT result =
			device.createShaderModule(&createInfo, device.GetAllocator(), &p_shader_modules[i]);
		VB_CHECK_VK_RESULT(result, "Failed to create shader module!");
		
		// Create shader stage info
		p_shader_stages[i] = vk::PipelineShaderStageCreateInfo{
			.stage				 = stage.stage,
			.module				 = p_shader_modules[i],
			.pName				 = stage.entry_point.data(),
			.pSpecializationInfo = &stage.specialization_info,
		};
	}
}

void CreateShaderModuleInfos(std::span<const PipelineStage> stages, std::vector<char>* p_bytes,
							 vk::ShaderModuleCreateInfo*		p_shader_module_create_infos,
							 vk::PipelineShaderStageCreateInfo* p_shader_stages) {
	for (auto [i, stage] : util::enumerate(stages)) {
		// Load or compile shader
		p_bytes[i] = LoadShader(stage);

		// Create shader module info
		p_shader_module_create_infos[i] = vk::ShaderModuleCreateInfo{
			.codeSize = p_bytes[i].size(),
			.pCode	  = reinterpret_cast<const u32*>(p_bytes[i].data()),
		};
		
		// Create shader stage info
		p_shader_stages[i] = vk::PipelineShaderStageCreateInfo{
			.pNext				 = &p_shader_module_create_infos[i],
			.stage				 = stage.stage,
			.pName				 = stage.entry_point.data(),
			.pSpecializationInfo = &stage.specialization_info,
		};
	}
}

void CreateVertexDescriptionsFromAttributes(std::span<vk::Format const>			 vertex_attributes,
											vk::VertexInputAttributeDescription* p_attribute_descs,
											u32*								 p_stride) {
	u32 attribute_size = 0;
	for (auto [i, format] : util::enumerate(vertex_attributes)) {
		p_attribute_descs[i] = vk::VertexInputAttributeDescription{.location = static_cast<u32>(i),
																   .binding	 = 0,
																   .format	 = vk::Format(format),
																   .offset	 = attribute_size};
		switch (format) {
		case vk::Format::eR32G32Sfloat:
			attribute_size += 2 * sizeof(float);
			break;
		case vk::Format::eR32G32B32Sfloat:
			attribute_size += 3 * sizeof(float);
			break;
		case vk::Format::eR32G32B32A32Sfloat:
			attribute_size += 4 * sizeof(float);
			break;
		default:
			VB_ASSERT(false, "Invalid Vertex Attribute");
			break;
		}
	}
	*p_stride = attribute_size;
}

} // namespace VB_NAMESPACE
