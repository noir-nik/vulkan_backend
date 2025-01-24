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

#include "vulkan_backend/interface/pipeline_library.hpp"
#include "vulkan_backend/interface/instance.hpp"
#include "vulkan_backend/interface/device.hpp"
#include "vulkan_backend/interface/descriptor.hpp"
#include "vulkan_backend/compile_shader.hpp"
#include "vulkan_backend/vk_result.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/util/conversion.hpp"

namespace VB_NAMESPACE {



void PipelineLibrary::Free() {
	for (auto& [_, pipeline]: vertex_input_interfaces) {
		device->destroyPipeline(pipeline, device->GetAllocator());
	}
	for (auto& [_, pipeline]: pre_rasterization_shaders) {
		device->destroyPipeline(pipeline, device->GetAllocator());
	}
	for (auto& [_, pipeline]: fragment_output_interfaces) {
		device->destroyPipeline(pipeline, device->GetAllocator());
	}
	for (auto& [_, pipeline]: fragment_shaders) {
		device->destroyPipeline(pipeline, device->GetAllocator());
	}

	vertex_input_interfaces.clear();
	pre_rasterization_shaders.clear();
	fragment_output_interfaces.clear();
	fragment_shaders.clear();
};

auto PipelineLibrary::CreatePipeline(GraphicsPipelineInfo const& info) -> Pipeline {
	VertexInputInfo vertexInputInfo{
		.vertex_attributes = info.vertex_attributes,
		.input_assembly	   = info.input_assembly
	};

	PreRasterizationInfo preRasterizationInfo {
		.layout         = info.layout,
		.dynamic_states = info.dynamic_states,
		.stages         = info.stages,
		.tessellation   = info.tessellation,
		.viewport       = info.viewport,
		.rasterization  = info.rasterization,
	};

	FragmentShaderInfo fragmentShaderInfo { 
		.layout         = info.layout, 
		.stages         = info.stages,
		.depth_stencil  = info.depth_stencil
	};

	FragmentOutputInfo fragmentOutputInfo {
		.blend_attachments = info.blend_attachments,
		.color_blend = info.color_blend,
		.multisample = info.multisample,
		.dynamic_states = info.dynamic_states,
		.color_formats = info.color_formats,
		.depth_format = info.depth_format,
		.stencil_format = info.stencil_format,
	};

	auto it_fragment_stage = std::find_if(info.stages.begin(), info.stages.end(), [](PipelineStage const& stage) {
		return stage.stage == vk::ShaderStageFlagBits::eFragment;
	});
	VB_ASSERT(it_fragment_stage != info.stages.begin(), "Graphics pipeline should have pre-rasterization stage");
	VB_ASSERT(it_fragment_stage != info.stages.end(),   "Graphics pipeline should have fragment stage");

	preRasterizationInfo.stages = {info.stages.begin(), it_fragment_stage};
	fragmentShaderInfo.stages   = {it_fragment_stage, info.stages.end()};

	auto it_vertex_input			  = vertex_input_interfaces.find(vertexInputInfo);
	auto it_pre_rasterization		  = pre_rasterization_shaders.find(preRasterizationInfo);
	auto it_fragment_shader			  = fragment_shaders.find(fragmentShaderInfo);
	auto it_fragment_output_interface = fragment_output_interfaces.find(fragmentOutputInfo);

	vk::Pipeline pipeline = LinkPipeline({
			(it_vertex_input != vertex_input_interfaces.end() ? it_vertex_input->second : CreateVertexInputInterface(vertexInputInfo)),
			(it_pre_rasterization != pre_rasterization_shaders.end() ? it_pre_rasterization->second : CreatePreRasterizationShaders(preRasterizationInfo)),
			(it_fragment_output_interface != fragment_output_interfaces.end() ? it_fragment_output_interface->second : CreateFragmentOutputInterface(fragmentOutputInfo)),
			(it_fragment_shader != fragment_shaders.end() ? it_fragment_shader->second : CreateFragmentShader(fragmentShaderInfo))
		},
		info.layout,
		std::all_of(info.stages.begin(), info.stages.end(), [](PipelineStage const& stage) {
			return (stage.flags & PipelineStage::Flags::kLinkTimeOptimization) ==
				PipelineStage::Flags::kLinkTimeOptimization;
		})
	);

	return {device->shared_from_this(), pipeline, info.layout, vk::PipelineBindPoint::eGraphics, info.name};
}

auto PipelineLibrary::CreateVertexInputInterface(VertexInputInfo const& info) -> vk::Pipeline {
	vk::GraphicsPipelineLibraryCreateInfoEXT library_info{
		.flags = vk::GraphicsPipelineLibraryFlagBitsEXT::eVertexInputInterface,
	};

	VB_VLA(vk::VertexInputAttributeDescription, attribute_descs,
		   std::max(info.vertex_attributes.size(), size_t(1)));
	u32 attributeSize = 0;
	for (u32 i = 0; auto& format : info.vertex_attributes) {
		attribute_descs[i] = vk::VertexInputAttributeDescription{
			.location = i,
			.binding = 0, 
			.format = vk::Format(format),
			.offset = attributeSize
		};
		switch (format) {
		case vk::Format::eR32G32Sfloat:
			attributeSize += 2 * sizeof(float);
			break;
		case vk::Format::eR32G32B32Sfloat:
			attributeSize += 3 * sizeof(float);
			break;
		case vk::Format::eR32G32B32A32Sfloat:
			attributeSize += 4 * sizeof(float);
			break;
		default:
			VB_ASSERT(false, "Invalid Vertex Attribute");
			break;
		}
		++i;
	}

	vk::VertexInputBindingDescription bindingDescription{
		.binding   = 0,
		.stride    = attributeSize,
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
	VB_VK_RESULT result = device->createGraphicsPipelines(device->pipeline_cache, 1, 
		&pipeline_library_info, device->GetAllocator(), &pipeline);
	VB_CHECK_VK_RESULT(result, "Failed to create vertex input interface!");
	vertex_input_interfaces.emplace(info, pipeline);
	return pipeline;
}

auto PipelineLibrary::CreatePreRasterizationShaders(PreRasterizationInfo const& info) -> vk::Pipeline {
	vk::GraphicsPipelineLibraryCreateInfoEXT library_info{
		.flags = vk::GraphicsPipelineLibraryFlagBitsEXT::ePreRasterizationShaders,
	};

	// Using the pipeline library extension, we can skip the pipeline shader module creation and directly pass
	// the shader code to the pipeline
	VB_VLA(vk::ShaderModuleCreateInfo, shader_module_infos, info.stages.size());
	VB_VLA(vk::PipelineShaderStageCreateInfo, shader_stages, info.stages.size());
	VB_VLA(std::vector<char>, bytes, info.stages.size());
	Pipeline::CreateShaderModuleInfos({info.stages, bytes, shader_module_infos, shader_stages});
	vk::PipelineDynamicStateCreateInfo dynamic_info = convert::DynamicStateInfo(info.dynamic_states);
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
	VB_VK_RESULT result = device->createGraphicsPipelines(device->pipeline_cache, 1,
		&pipeline_library_info, device->GetAllocator(), &pipeline);
	VB_CHECK_VK_RESULT(result, "Failed to create pre-rasterization shaders!");
	pre_rasterization_shaders.emplace(info, pipeline);
	return pipeline;
}

auto PipelineLibrary::CreateFragmentShader(FragmentShaderInfo const& info) -> vk::Pipeline {
	vk::GraphicsPipelineLibraryCreateInfoEXT library_info{
		.flags = vk::GraphicsPipelineLibraryFlagBitsEXT::eFragmentShader,
	};

	VB_VLA(vk::ShaderModuleCreateInfo, shader_module_infos, info.stages.size());
	VB_VLA(vk::PipelineShaderStageCreateInfo, shader_stages, info.stages.size());
	VB_VLA(std::vector<char>, bytes, info.stages.size());
	Pipeline::CreateShaderModuleInfos({info.stages, bytes, shader_module_infos, shader_stages});

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
	VB_VK_RESULT result = device->createGraphicsPipelines(device->pipeline_cache, 1,
		&pipeline_library_info, device->GetAllocator(), &pipeline);
	VB_CHECK_VK_RESULT(result, "Failed to create fragment shader!");
	fragment_shaders.emplace(info, pipeline);
	return pipeline;
}

auto PipelineLibrary::CreateFragmentOutputInterface(FragmentOutputInfo const& info) -> vk::Pipeline {
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
	VB_VK_RESULT result = device->createGraphicsPipelines(device->pipeline_cache, 1,
		&pipeline_library_info, nullptr, &pipeline);
	VB_CHECK_VK_RESULT(result, "Failed to create fragment output interface!");
	fragment_output_interfaces.emplace(info, pipeline);
	return pipeline;
}

auto PipelineLibrary::LinkPipeline(std::array<vk::Pipeline, 4> const& pipelines, vk::PipelineLayout layout, bool link_time_optimization) -> vk::Pipeline {

	// Link the library parts into a graphics pipeline
	vk::PipelineLibraryCreateInfoKHR linkingInfo {
		.libraryCount = static_cast<u32>(pipelines.size()),
		.pLibraries   = pipelines.data(),
	};

	vk::GraphicsPipelineCreateInfo pipelineInfo {
		.pNext  = &linkingInfo,
		.layout = layout,
	};

	if (link_time_optimization) {
		pipelineInfo.flags = vk::PipelineCreateFlagBits::eLinkTimeOptimizationEXT;
	}

	//todo: Thread pipeline cache
	vk::Pipeline pipeline;
	VB_VK_RESULT result = device->createGraphicsPipelines(device->pipeline_cache, 1, &pipelineInfo, nullptr, &pipeline);
	VB_CHECK_VK_RESULT(result, "Failed to link pipeline!");
	return pipeline;
}

}; // namespace VB_NAMESPACE