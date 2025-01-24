#ifndef VB_USE_STD_MODULE
#include <algorithm>
#include <utility>
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include <vulkan/vulkan.h>

#include "vulkan_backend/compile_shader.hpp"
#include "vulkan_backend/interface/device.hpp"
#include "vulkan_backend/interface/instance.hpp"
#include "vulkan_backend/interface/pipeline.hpp"
#include "vulkan_backend/util/enumerate.hpp"
#include "vulkan_backend/log.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/vk_result.hpp"

namespace VB_NAMESPACE {

Pipeline::Pipeline(std::shared_ptr<Device> device, vk::Pipeline pipeline, vk::PipelineLayout layout,
				   vk::PipelineBindPoint point, std::string_view name)
	: vk::Pipeline(pipeline), Named(name), ResourceBase(device), layout(layout), point(point) {}

Pipeline::Pipeline(std::shared_ptr<Device> device, PipelineInfo const& info) : ResourceBase(device) {
	Create(info);
}

Pipeline::Pipeline(std::shared_ptr<Device> device, GraphicsPipelineInfo const& info) : ResourceBase(device) {
	Create(info);
}

Pipeline::Pipeline(Pipeline&& other)
	: vk::Pipeline(other), Named(std::move(other)), ResourceBase<Device>(std::exchange(other.owner, {})),
	  layout(std::exchange(other.layout, {})), point(std::exchange(other.point, {})) {
	static_cast<vk::Pipeline&>(other) = vk::Pipeline{};
}

Pipeline& Pipeline::operator=(Pipeline&& other) {
	if (this != &other) {
		Free();
		static_cast<vk::Pipeline&>(*this) = std::exchange(static_cast<vk::Pipeline&>(other), {});
		Named::operator=(std::move(other));
		ResourceBase::operator=(std::move(other));
		layout = std::exchange(other.layout, {});
		point  = std::move(other.point);
	}
	return *this;
}

void Pipeline::CreateShaderStages(PipelineStageCreateInfo const& info) {
	for (auto i = 0; auto& stage : info.stages) {
		std::vector<char>		   bytes = LoadShader(stage);
		vk::ShaderModuleCreateInfo createInfo{
			.codeSize = bytes.size(),
			.pCode	  = reinterpret_cast<const u32*>(bytes.data()),
		};
		VB_VK_RESULT result = info.device->createShaderModule(&createInfo, info.device->GetAllocator(),
														&info.p_shader_modules[i]);
		VB_CHECK_VK_RESULT(result, "Failed to create shader module!");
		info.p_shader_stages[i] = vk::PipelineShaderStageCreateInfo{
			.stage				 = stage.stage,
			.module				 = info.p_shader_modules[i],
			.pName				 = stage.entry_point.data(),
			.pSpecializationInfo = &stage.specialization_info,
		};
		++i;
	}
}

void Pipeline::CreateShaderModuleInfos(PipelineShaderModuleCreateInfo const& info) {
	for (auto [i, stage] : util::enumerate(info.stages)) {
		info.p_bytes[i] = LoadShader(stage);

		info.p_shader_module_create_infos[i] = vk::ShaderModuleCreateInfo{
			.codeSize = info.p_bytes[i].size(),
			.pCode    = reinterpret_cast<const u32*>(info.p_bytes[i].data()),
		};

		info.p_shader_stages[i] = vk::PipelineShaderStageCreateInfo {
			.pNext = &info.p_shader_module_create_infos[i],
			.stage = stage.stage,
			.pName = stage.entry_point.data(),
			.pSpecializationInfo = nullptr,
		};
	}
}

// Compute pipeline
void Pipeline::Create(PipelineInfo const& info) {
	this->layout = info.layout == vk::PipelineLayout{} ? owner->GetBindlessPipelineLayout() : info.layout;
	this->point  = vk::PipelineBindPoint::eCompute;
	VB_ASSERT(info.stages.size() == 1, "Compute pipeline supports only 1 stage.");
	vk::ShaderModule shader_modules[1];
	vk::PipelineShaderStageCreateInfo shader_stages[1];
	CreateShaderStages({owner.get(), info.stages, shader_modules, shader_stages});

	vk::ComputePipelineCreateInfo pipelineInfo{
		.pNext				= VK_NULL_HANDLE,
		.stage				= shader_stages[0],
		.layout				= layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex	= -1,
	};
	VB_VK_RESULT result = owner->createComputePipelines(owner->pipeline_cache, 1, &pipelineInfo,
														owner->GetAllocator(), this);
	VB_CHECK_VK_RESULT(result, "Failed to create compute pipeline!");
	for (auto& shaderModule : shader_modules) {
		owner->destroyShaderModule(shaderModule, owner->GetAllocator());
	}
}

// Graphics pipeline
void Pipeline::Create(GraphicsPipelineInfo const& info) {
	this->layout = info.layout == vk::PipelineLayout{} ? owner->GetBindlessPipelineLayout() : info.layout;
	this->point  = vk::PipelineBindPoint::eGraphics;
	VB_ASSERT(info.stages.size() == 1, "Compute pipeline supports only 1 stage.");
	VB_VLA(vk::ShaderModule, shader_modules, info.stages.size());
	VB_VLA(vk::PipelineShaderStageCreateInfo, shader_stages, info.stages.size());
	CreateShaderStages({owner.get(), info.stages, shader_modules, shader_stages});

	VB_VLA(vk::VertexInputAttributeDescription, attribute_descs, info.vertex_attributes.size());
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
		.stride	   = attributeSize,
		.inputRate = vk::VertexInputRate::eVertex,
	};

	vk::PipelineVertexInputStateCreateInfo vertex_input_info{
		.vertexBindingDescriptionCount	 = 1,
		.pVertexBindingDescriptions		 = &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<u32>(info.vertex_attributes.size()),
		.pVertexAttributeDescriptions	 = attribute_descs.data(),
	};

	vk::PipelineDynamicStateCreateInfo dynamicInfo{
		.dynamicStateCount = static_cast<u32>(info.dynamic_states.size()),
		.pDynamicStates	   = reinterpret_cast<const vk::DynamicState*>(info.dynamic_states.data()),
	};

	vk::PipelineRenderingCreateInfo pipeline_rendering_info{
		.viewMask				 = 0,
		.colorAttachmentCount	 = static_cast<u32>(info.color_formats.size()),
		.pColorAttachmentFormats = reinterpret_cast<const vk::Format*>(info.color_formats.data()),
		.depthAttachmentFormat	 = info.depth_format,
		.stencilAttachmentFormat = info.stencil_format,
	};

	// if we have less blend attachments, than color attachments, just fill with the first one
	VB_VLA(vk::PipelineColorBlendAttachmentState, blendAttachments, info.color_formats.size());
	auto end = std::copy_n(info.blend_attachments.begin(),
						   std::min(info.blend_attachments.size(), info.color_formats.size()),
						   blendAttachments.begin());
	std::fill(end, blendAttachments.end(), defaults::kBlendAttachment);

	vk::PipelineColorBlendStateCreateInfo colorBlendState{
		.logicOpEnable	 = info.color_blend.logicOpEnable,
		.logicOp		 = info.color_blend.logicOp,
		.attachmentCount = static_cast<u32>(blendAttachments.size()),
		.pAttachments	 = blendAttachments.data(),
		.blendConstants	 = info.color_blend.blendConstants,
	};

	vk::GraphicsPipelineCreateInfo pipeline_info {
		.pNext				 = &pipeline_rendering_info,
		.stageCount			 = static_cast<u32>(shader_stages.size()),
		.pStages			 = shader_stages.data(),
		.pVertexInputState	 = &vertex_input_info,
		.pInputAssemblyState = &info.input_assembly,
		.pTessellationState	 = info.tessellation ? &info.tessellation.value() : nullptr,
		.pViewportState		 = &info.viewport,
		.pRasterizationState = &info.rasterization,
		.pMultisampleState	 = &info.multisample,
		.pDepthStencilState	 = &info.depth_stencil,
		.pColorBlendState	 = &colorBlendState,
		.pDynamicState		 = &dynamicInfo,
		.layout				 = info.layout ? info.layout : owner->bindless_pipeline_layout,
		.basePipelineHandle	 = nullptr,
		.basePipelineIndex	 = -1,
	};

	VB_VK_RESULT result = owner->createGraphicsPipelines(owner->pipeline_cache, 1, &pipeline_info,
														owner->GetAllocator(), this);
	VB_CHECK_VK_RESULT(result, "Failed to create graphics pipeline!");

	for (auto& shaderModule : shader_modules) {
		owner->destroyShaderModule(shaderModule, owner->GetAllocator());
	}
}

auto Pipeline::GetResourceTypeName() const -> char const* { return "PipelineResource"; }

Pipeline::~Pipeline() { Free(); }

void Pipeline::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), GetName());
	owner->destroyPipeline(*this, owner->GetAllocator());
	// Layout is destroyed by device
	// owner->destroyPipelineLayout(layout, owner->GetAllocator());
}
}; // namespace VB_NAMESPACE