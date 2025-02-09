#ifndef VB_USE_STD_MODULE
#include <algorithm>
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

// #include <vulkan/vulkan.h>

#include "vulkan_backend/interface/device/device.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/util/conversion.hpp"
#include "vulkan_backend/util/pipeline.hpp"
#include "vulkan_backend/vk_result.hpp"


namespace VB_NAMESPACE {
auto SplitPipelineInfo(GraphicsPipelineInfo const& info) -> std::tuple<VertexInputInfo, PreRasterizationInfo, FragmentShaderInfo, FragmentOutputInfo> {
	VertexInputInfo vertexInputInfo{
		.vertex_attributes = info.vertex_attributes,
		.input_assembly	   = info.input_assembly
	};

	PreRasterizationInfo pre_rasterization_info {
		.layout         = info.layout,
		.dynamic_states = info.dynamic_states,
		.stages         = info.stages,
		.tessellation   = info.tessellation,
		.viewport       = info.viewport,
		.rasterization  = info.rasterization,
	};

	FragmentShaderInfo fragment_shader_info { 
		.layout         = info.layout, 
		.stages         = info.stages,
		.depth_stencil  = info.depth_stencil
	};

	FragmentOutputInfo fragment_output_info {
		.blend_attachments = info.blend_attachments,
		.color_blend = info.color_blend,
		.multisample = info.multisample,
		.dynamic_states = info.dynamic_states,
		.color_formats = info.color_formats,
		.depth_format = info.depth_format,
		.stencil_format = info.stencil_format,
	};

	return {vertexInputInfo, pre_rasterization_info, fragment_shader_info, fragment_output_info};
}

auto Device::CreateCommand(u32 queue_family_index) -> Command {
	Command command;
	command.Create(*this, queue_family_index);
	return command;
}

auto Device::CreateVertexInputInterface(VertexInputInfo const& info) -> vk::Pipeline {
	vk::GraphicsPipelineLibraryCreateInfoEXT library_info{
		.flags = vk::GraphicsPipelineLibraryFlagBitsEXT::eVertexInputInterface,
	};

	VB_VLA(vk::VertexInputAttributeDescription, attribute_descs,
		   std::max(info.vertex_attributes.size(), size_t(1)));
	u32 attribute_size = 0;
	CreateVertexDescriptionsFromAttributes(info.vertex_attributes, attribute_descs.data(), &attribute_size);

	vk::VertexInputBindingDescription bindingDescription{
		.binding   = 0,
		.stride    = attribute_size,
		.inputRate = vk::VertexInputRate::eVertex,
	};

	vk::PipelineVertexInputStateCreateInfo vertex_input_info{
		.vertexBindingDescriptionCount	 = 1,
		.pVertexBindingDescriptions		 = &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<u32>(info.vertex_attributes.size()),
		.pVertexAttributeDescriptions	 = attribute_descs.data(),
	};

	vk::GraphicsPipelineCreateInfo pipeline_library_info {
		.pNext               = &library_info,
		.flags               = vk::PipelineCreateFlagBits::eLibraryKHR
								| vk::PipelineCreateFlagBits::eRetainLinkTimeOptimizationInfoEXT,
		.pVertexInputState   = &vertex_input_info,
		.pInputAssemblyState = &info.input_assembly,
		.pDynamicState       = nullptr
	};

	vk::Pipeline pipeline;
	VB_VK_RESULT result = createGraphicsPipelines(pipeline_cache, 1, 
		&pipeline_library_info, GetAllocator(), &pipeline);
	VB_CHECK_VK_RESULT(result, "Failed to create vertex input interface!");
	return pipeline;
}

auto Device::CreatePreRasterizationShaders(PreRasterizationInfo const& info) -> vk::Pipeline {
	vk::GraphicsPipelineLibraryCreateInfoEXT library_info{
		.flags = vk::GraphicsPipelineLibraryFlagBitsEXT::ePreRasterizationShaders,
	};

	// Using the pipeline library extension, we can skip the pipeline shader module creation and directly pass
	// the shader code to the pipeline
	VB_VLA(vk::ShaderModuleCreateInfo, shader_module_infos, info.stages.size());
	VB_VLA(vk::PipelineShaderStageCreateInfo, shader_stages, info.stages.size());
	VB_VLA(std::vector<char>, bytes, info.stages.size());
	CreateShaderModuleInfos(info.stages, bytes.data(), shader_module_infos.data(), shader_stages.data());
	
	vk::PipelineDynamicStateCreateInfo dynamic_info = util::DynamicStateInfo(info.dynamic_states);
	vk::GraphicsPipelineCreateInfo pipeline_library_info {
		.pNext               = &library_info,
		.flags               = vk::PipelineCreateFlagBits::eLibraryKHR |
								vk::PipelineCreateFlagBits::eRetainLinkTimeOptimizationInfoEXT,
		.stageCount          = static_cast<u32>(shader_stages.size()),
		.pStages             = shader_stages.data(),
		.pViewportState      = &info.viewport,
		.pRasterizationState = &info.rasterization,
		.pDynamicState       = &dynamic_info,
		.layout              = info.layout,
	};
	vk::Pipeline pipeline;
	VB_VK_RESULT result = createGraphicsPipelines(pipeline_cache, 1,
		&pipeline_library_info, GetAllocator(), &pipeline);
	VB_CHECK_VK_RESULT(result, "Failed to create pre-rasterization shaders!");
	return pipeline;
}

auto Device::CreateFragmentShader(FragmentShaderInfo const& info) -> vk::Pipeline {
	vk::GraphicsPipelineLibraryCreateInfoEXT library_info{
		.flags = vk::GraphicsPipelineLibraryFlagBitsEXT::eFragmentShader,
	};

	VB_VLA(vk::ShaderModuleCreateInfo, shader_module_infos, info.stages.size());
	VB_VLA(vk::PipelineShaderStageCreateInfo, shader_stages, info.stages.size());
	VB_VLA(std::vector<char>, bytes, info.stages.size());
	CreateShaderModuleInfos(info.stages, bytes.data(), shader_module_infos.data(), shader_stages.data());

	vk::GraphicsPipelineCreateInfo pipeline_library_info {
		.pNext              = &library_info,
		.flags              = vk::PipelineCreateFlagBits::eLibraryKHR |
								vk::PipelineCreateFlagBits::eRetainLinkTimeOptimizationInfoEXT,
		.stageCount          = static_cast<u32>(shader_stages.size()),
		.pStages             = shader_stages.data(),
		.pDepthStencilState = &info.depth_stencil,
		.layout             = info.layout,
	};

	//todo: Thread pipeline cache
	vk::Pipeline pipeline;
	VB_VK_RESULT result = createGraphicsPipelines(pipeline_cache, 1,
		&pipeline_library_info, GetAllocator(), &pipeline);
	VB_CHECK_VK_RESULT(result, "Failed to create fragment shader!");
	return pipeline;
}

auto Device::CreateFragmentOutputInterface(FragmentOutputInfo const& info) -> vk::Pipeline {
	vk::GraphicsPipelineLibraryCreateInfoEXT library_info {
		.flags = vk::GraphicsPipelineLibraryFlagBitsEXT::eFragmentOutputInterface,
	};

	VB_VLA(vk::PipelineColorBlendAttachmentState, blend_attachments, info.color_formats.size());
	auto end = std::copy_n(info.blend_attachments.begin(),
						   std::min(info.blend_attachments.size(), info.color_formats.size()),
						   blend_attachments.begin());
	std::fill(end, blend_attachments.end(), defaults::kBlendAttachment);

	vk::PipelineColorBlendStateCreateInfo colorBlendState = info.color_blend;
	colorBlendState.attachmentCount = static_cast<u32>(blend_attachments.size());
	colorBlendState.pAttachments = blend_attachments.data();

	vk::PipelineRenderingCreateInfo pipeline_rendering_info {
		.pNext                   = &library_info,
		.viewMask                = 0,
		.colorAttachmentCount    = static_cast<u32>(info.color_formats.size()),
		.pColorAttachmentFormats = reinterpret_cast<vk::Format const*>(info.color_formats.data()),
		.depthAttachmentFormat   = info.depth_format,
		.stencilAttachmentFormat = info.stencil_format,
	};

	vk::GraphicsPipelineCreateInfo pipeline_library_info{
		.pNext = &pipeline_rendering_info,
		.flags = vk::PipelineCreateFlagBits::eLibraryKHR
				| vk::PipelineCreateFlagBits::eRetainLinkTimeOptimizationInfoEXT,
		.pMultisampleState = &info.multisample,
		.pColorBlendState = &colorBlendState,
		// .layout = info.layout,
	};

	vk::Pipeline pipeline;
	VB_VK_RESULT result = createGraphicsPipelines(pipeline_cache, 1,
		&pipeline_library_info, nullptr, &pipeline);
	VB_CHECK_VK_RESULT(result, "Failed to create fragment output interface!");
	return pipeline;
}

auto Device::LinkPipeline(std::array<vk::Pipeline, 4> const& pipelines, vk::PipelineLayout layout, bool link_time_optimization) -> vk::Pipeline {

	// Link the library parts into a graphics pipeline
	vk::PipelineLibraryCreateInfoKHR linking_info {
		.libraryCount = static_cast<u32>(pipelines.size()),
		.pLibraries   = pipelines.data(),
	};

	vk::GraphicsPipelineCreateInfo pipeline_info {
		.pNext  = &linking_info,
		.layout = layout,
	};

	if (link_time_optimization) {
		pipeline_info.flags = vk::PipelineCreateFlagBits::eLinkTimeOptimizationEXT;
	}

	//todo: Thread pipeline cache
	vk::Pipeline pipeline;
	VB_VK_RESULT result = createGraphicsPipelines(pipeline_cache, 1, &pipeline_info, nullptr, &pipeline);
	VB_CHECK_VK_RESULT(result, "Failed to link pipeline!");
	return pipeline;
}

}; // namespace VB_NAMESPACE