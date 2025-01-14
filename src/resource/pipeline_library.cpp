#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#else
import std;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include <vulkan/vulkan.h>

#include "pipeline_library.hpp"
#include "instance.hpp"
#include "pipeline.hpp"
#include "device.hpp"
#include "descriptor.hpp"
#include "../compile_shader.hpp"

namespace VB_NAMESPACE {

void PipelineLibrary::Free() {
	for (auto& [_, pipeline]: vertexInputInterfaces) {
		vkDestroyPipeline(device->handle, pipeline, device->owner->allocator);
	}
	for (auto& [_, pipeline]: preRasterizationShaders) {
		vkDestroyPipeline(device->handle, pipeline, device->owner->allocator);
	}
	for (auto& [_, pipeline]: fragmentOutputInterfaces) {
		vkDestroyPipeline(device->handle, pipeline, device->owner->allocator);
	}
	for (auto& [_, pipeline]: fragmentShaders) {
		vkDestroyPipeline(device->handle, pipeline, device->owner->allocator);
	}

	vertexInputInterfaces.clear();
	preRasterizationShaders.clear();
	fragmentOutputInterfaces.clear();
	fragmentShaders.clear();
};

auto PipelineLibrary::CreatePipeline(PipelineInfo const& info) -> Pipeline {
	Pipeline pipeline;
	pipeline.resource = MakeResource<PipelineResource>(device, info.name);
	pipeline.resource->point = info.point;
	pipeline.resource->CreateLayout({{device->descriptor.resource->layout}});

	PipelineLibrary::VertexInputInfo vertexInputInfo { info.vertexAttributes, info.line_topology };
	if (!vertexInputInterfaces.count(vertexInputInfo)) {
		CreateVertexInputInterface(vertexInputInfo);
	}

	PipelineLibrary::PreRasterizationInfo preRasterizationInfo {
		.pipelineLayout = pipeline.resource->layout,
		.lineTopology   = info.line_topology,
		.cullMode       = info.cullMode 
	};
	PipelineLibrary::FragmentShaderInfo   fragmentShaderInfo   { pipeline.resource->layout, info.samples};

	auto it_fragment = std::find_if(info.stages.begin(), info.stages.end(), [](Pipeline::Stage const& stage) {
		return stage.stage == ShaderStage::eFragment;
	});
	VB_ASSERT(it_fragment != info.stages.begin(), "Graphics pipeline should have pre-rasterization stage");
	VB_ASSERT(it_fragment != info.stages.end(),   "Graphics pipeline should have fragment stage");

	preRasterizationInfo.stages = {info.stages.begin(), it_fragment};
	fragmentShaderInfo.stages   = {it_fragment, info.stages.end()};

	if (!preRasterizationShaders.contains(preRasterizationInfo)) {
		CreatePreRasterizationShaders(preRasterizationInfo);
	}
	if (!fragmentShaders.contains(fragmentShaderInfo)) {
		CreateFragmentShader(fragmentShaderInfo);
	}

	PipelineLibrary::FragmentOutputInfo fragmentOutputInfo {
		.pipelineLayout = pipeline.resource->layout,
		.colorFormats   = info.color_formats,
		.useDepth       = info.use_depth,
		.depthFormat    = info.depth_format,
		.samples        = info.samples
	};

	if (!fragmentOutputInterfaces.contains(fragmentOutputInfo)) {
		CreateFragmentOutputInterface(fragmentOutputInfo);
	}

	pipeline.resource->handle = LinkPipeline({
			vertexInputInterfaces[vertexInputInfo],
			preRasterizationShaders[preRasterizationInfo],
			fragmentShaders[fragmentShaderInfo],
			fragmentOutputInterfaces[fragmentOutputInfo]
		},
		pipeline.resource->layout
	);

	return pipeline;
}

void PipelineLibrary::CreateVertexInputInterface(VertexInputInfo const& info) {
	VkGraphicsPipelineLibraryCreateInfoEXT libraryInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
		.flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT,
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = info.lineTopology? VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		// with this parameter true we can break up lines and triangles in _STRIP topology modes
		.primitiveRestartEnable = VK_FALSE,
	};

	// We use std::vector, because vertexAttributes can be empty and vla with size 0 is undefined behavior
	std::vector<VkVertexInputAttributeDescription> attributeDescs(info.vertexAttributes.size());
	u32 attributeSize = 0;
	for (u32 i = 0; auto& format: info.vertexAttributes) {
		attributeDescs[i] = VkVertexInputAttributeDescription{
			.location = i,
			.binding = 0,
			.format = VkFormat(format),
			.offset = attributeSize
		};
		switch (format) {
		case Format::eR32G32Sfloat:       attributeSize += 2 * sizeof(float); break;
		case Format::eR32G32B32Sfloat:    attributeSize += 3 * sizeof(float); break;
		case Format::eR32G32B32A32Sfloat: attributeSize += 4 * sizeof(float); break;
		default: VB_ASSERT(false, "Invalid Vertex Attribute"); break;
		}
		i++;
	}

	VkVertexInputBindingDescription bindingDescription{
		.binding   = 0,
		.stride    = attributeSize,
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<u32>(attributeDescs.size()),
		.pVertexAttributeDescriptions = attributeDescs.data(),
	};

	VkGraphicsPipelineCreateInfo pipelineLibraryInfo {
		.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext               = &libraryInfo,
		.flags               = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR
								| VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT,
		.pVertexInputState   = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pDynamicState       = nullptr
	};

	VkPipeline pipeline;
	VB_VK_RESULT res = vkCreateGraphicsPipelines(device->handle, device->pipelineCache, 1, 
		&pipelineLibraryInfo, device->owner->allocator, &pipeline);
	VB_CHECK_VK_RESULT(device->owner->init_info.checkVkResult, res, "Failed to create vertex input interface!");
	vertexInputInterfaces.emplace(info, pipeline);
}

void PipelineLibrary::CreatePreRasterizationShaders(PreRasterizationInfo const& info) {
	VkGraphicsPipelineLibraryCreateInfoEXT libraryInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
		.flags = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT,
	};

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	if (info.lineTopology) {
		dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
	}

	VkPipelineDynamicStateCreateInfo dynamicInfo{
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<u32>(dynamicStates.size()),
		.pDynamicStates    = dynamicStates.data(),
	};

	VkPipelineViewportStateCreateInfo viewportState{
		.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports    = nullptr,
		.scissorCount  = 1,
		.pScissors     = nullptr,
	};

	VkPipelineRasterizationStateCreateInfo rasterizationState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE, // fragments beyond near and far planes are clamped to them
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = (VkCullModeFlags)info.cullMode, // line thickness in terms of number of fragments
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f,
	};

	// Using the pipeline library extension, we can skip the pipeline shader module creation and directly pass the shader code to the pipeline
	// std::vector<VkShaderModuleCreateInfo> shaderModuleInfos;
	// shaderModuleInfos.reserve(info.stages.size());
	// std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
	// shader_stages.reserve(info.stages.size());
	// std::vector<std::vector<char>> bytes;
	// bytes.reserve(info.stages.size());
	// for (auto& stage: info.stages) {
	// 	bytes.emplace_back(LoadShader(stage));

	// 	shaderModuleInfos.emplace_back(VkShaderModuleCreateInfo{
	// 		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	// 		.codeSize =  bytes.size(),
	// 		.pCode    = (const u32*)bytes.data()
	// 	});

	// 	shader_stages.emplace_back(VkPipelineShaderStageCreateInfo {
	// 		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	// 		.pNext = &shaderModuleInfos.back(),
	// 		.stage = (VkShaderStageFlagBits)stage.stage,
	// 		.pName = stage.entryPoint.data(),
	// 		.pSpecializationInfo = nullptr,
	// 	});
	// }

	std::vector<VkShaderModuleCreateInfo> shaderModuleInfos(info.stages.size());
	std::vector<VkPipelineShaderStageCreateInfo> shader_stages(info.stages.size());
	std::vector<std::vector<char>> bytes(info.stages.size());
	for (size_t i = 0; i < info.stages.size(); ++i) {
		auto& stage = info.stages[i];

		bytes[i] = LoadShader(stage);

		shaderModuleInfos[i] = VkShaderModuleCreateInfo{
			.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = bytes[i].size(),
			.pCode    = (const u32*)bytes[i].data()
		};

		shader_stages[i] = VkPipelineShaderStageCreateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = &shaderModuleInfos[i],
			.stage = (VkShaderStageFlagBits)stage.stage,
			.pName = stage.entry_point.data(),
			.pSpecializationInfo = nullptr,
		};
	}

	VkGraphicsPipelineCreateInfo pipelineLibraryInfo {
		.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext               = &libraryInfo,
		.flags               = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR |
								VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT,
		.stageCount          = static_cast<u32>(shader_stages.size()),
		.pStages             = shader_stages.data(),
		.pViewportState      = &viewportState,
		.pRasterizationState = &rasterizationState,
		.pDynamicState       = &dynamicInfo,
		.layout              = info.pipelineLayout,
	};
	VkPipeline pipeline;
	VB_VK_RESULT res = vkCreateGraphicsPipelines(device->handle, device->pipelineCache, 1,
		&pipelineLibraryInfo, device->owner->allocator, &pipeline);
	VB_CHECK_VK_RESULT(device->owner->init_info.checkVkResult, res,
		"Failed to create pre-rasterization shaders!");
	preRasterizationShaders.emplace(info, pipeline);
}

void PipelineLibrary::CreateFragmentShader(FragmentShaderInfo const& info) {
	VkGraphicsPipelineLibraryCreateInfoEXT libraryInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
		.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT,
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.front = {},
		.back = {},
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f,
	};

	VkPipelineMultisampleStateCreateInfo multisampleState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = (VkSampleCountFlagBits)std::min(info.samples, device->physicalDevice->maxSamples),
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 0.5f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};


	std::vector<VkShaderModuleCreateInfo> shaderModuleInfos(info.stages.size());
	std::vector<VkPipelineShaderStageCreateInfo> shader_stages(info.stages.size());
	std::vector<std::vector<char>> bytes(info.stages.size());
	for (size_t i = 0; i < info.stages.size(); ++i) {
		auto& stage = info.stages[i];

		bytes[i] = LoadShader(stage);

		shaderModuleInfos[i] = VkShaderModuleCreateInfo{
			.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = bytes[i].size(),
			.pCode    = (const u32*)bytes[i].data()
		};

		shader_stages[i] = VkPipelineShaderStageCreateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = &shaderModuleInfos[i],
			.stage = (VkShaderStageFlagBits)stage.stage,
			.pName = stage.entry_point.data(),
			.pSpecializationInfo = nullptr,
		};
	}

	VkGraphicsPipelineCreateInfo pipelineLibraryInfo {
		.sType              = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext              = &libraryInfo,
		.flags              = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | 
								VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT,
		.stageCount          = static_cast<u32>(shader_stages.size()),
		.pStages             = shader_stages.data(),
		.pMultisampleState  = &multisampleState,
		.pDepthStencilState = &depthStencilState,
		.layout             = info.pipelineLayout,
	};

	//todo: Thread pipeline cache
	VkPipeline pipeline;
	VB_VK_RESULT res = vkCreateGraphicsPipelines(device->handle, device->pipelineCache, 1,
		&pipelineLibraryInfo, nullptr, &pipeline);
	VB_CHECK_VK_RESULT(device->owner->init_info.checkVkResult, res, "Failed to create vertex input interface!");
	fragmentShaders.emplace(info, pipeline);
}

void PipelineLibrary::CreateFragmentOutputInterface(FragmentOutputInfo const& info) {
	VkGraphicsPipelineLibraryCreateInfoEXT libraryInfo {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
		.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT,
	};

	std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(info.colorFormats.size(), {
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
						| VK_COLOR_COMPONENT_G_BIT
						| VK_COLOR_COMPONENT_B_BIT
						| VK_COLOR_COMPONENT_A_BIT,
	});

	VkPipelineColorBlendStateCreateInfo colorBlendState {
		.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable   = VK_FALSE,
		.logicOp         = VK_LOGIC_OP_COPY,
		.attachmentCount = static_cast<u32>(blendAttachments.size()),
		.pAttachments    = blendAttachments.data(),
		.blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
	};

	VkPipelineMultisampleStateCreateInfo multisampleState {
		.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples  = (VkSampleCountFlagBits)std::min(info.samples, device->physicalDevice->maxSamples),
		.sampleShadingEnable   = VK_FALSE,
		.minSampleShading      = 0.5f,
		.pSampleMask           = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable      = VK_FALSE,
	};

	VkPipelineRenderingCreateInfo pipelineRenderingInfo {
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		.pNext                   = &libraryInfo,
		.viewMask                = 0,
		.colorAttachmentCount    = static_cast<u32>(info.colorFormats.size()),
		.pColorAttachmentFormats = reinterpret_cast<VkFormat const*>(info.colorFormats.data()),
		.depthAttachmentFormat   = info.useDepth ? (VkFormat)info.depthFormat : VK_FORMAT_UNDEFINED,
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	};


	VkGraphicsPipelineCreateInfo pipelineLibraryInfo {
		.sType             = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext             = &pipelineRenderingInfo,
		.flags             = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT,
		.pMultisampleState = &multisampleState,
		.pColorBlendState  = &colorBlendState,
		.layout            = info.pipelineLayout,
	};

	VkPipeline pipeline;
	VB_VK_RESULT result = vkCreateGraphicsPipelines(device->handle, device->pipelineCache, 1,
		&pipelineLibraryInfo, nullptr, &pipeline);
	VB_CHECK_VK_RESULT(device->owner->init_info.checkVkResult, result, "Failed to create fragment output interface!");
	fragmentOutputInterfaces.emplace(info, pipeline);
}

auto PipelineLibrary::LinkPipeline(std::array<VkPipeline, 4> const& pipelines, VkPipelineLayout layout) -> VkPipeline {

	// Link the library parts into a graphics pipeline
	VkPipelineLibraryCreateInfoKHR linkingInfo {
		.sType        = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,
		.libraryCount = static_cast<u32>(pipelines.size()),
		.pLibraries   = pipelines.data(),
	};

	VkGraphicsPipelineCreateInfo pipelineInfo {
		.sType  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext  = &linkingInfo,
		.layout = layout,
	};

	if (device->init_info.link_time_optimization) {
		pipelineInfo.flags = VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;
	}

	//todo: Thread pipeline cache
	VkPipeline pipeline = VK_NULL_HANDLE;
	VB_VK_RESULT result = vkCreateGraphicsPipelines(device->handle, device->pipelineCache, 1, &pipelineInfo, nullptr, &pipeline);
	VB_CHECK_VK_RESULT(device->owner->init_info.checkVkResult, result, "Failed to create vertex input interface!");
	return pipeline;
}

}; // namespace VB_NAMESPACE