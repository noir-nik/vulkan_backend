#pragma once

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"
#include "vulkan_backend/classes/structs.hpp"

VB_EXPORT
namespace VB_NAMESPACE {	
namespace defaults {

constexpr inline vk::PipelineInputAssemblyStateCreateInfo kInputAssembly{
	.topology = vk::PrimitiveTopology::eTriangleList,
	.primitiveRestartEnable = vk::False,
};

constexpr inline vk::PipelineVertexInputStateCreateInfo kVertexInput{
	.vertexBindingDescriptionCount = 0,
	.pVertexBindingDescriptions    = nullptr,
	.vertexAttributeDescriptionCount = 0,
	.pVertexAttributeDescriptions    = nullptr,
};

// default viewport and scissor are dynamic
constexpr inline vk::PipelineViewportStateCreateInfo kOneDynamicViewportOneDynamicScissor{
	.viewportCount = 1,
	.pViewports    = nullptr,
	.scissorCount  = 1,
	.pScissors     = nullptr,
};

constexpr inline vk::PipelineRasterizationStateCreateInfo kRasterization{
	// .flags = {},
	// .depthClampEnable = {},
	// .rasterizerDiscardEnable = {},
	// .polygonMode = vk::PolygonMode::eFill,
	// .cullMode = {},
	.frontFace = vk::FrontFace::eClockwise,
	// .depthBiasEnable = {},
	// .depthBiasConstantFactor = {},
	// .depthBiasClamp = {},
	// .depthBiasSlopeFactor = {},
	.lineWidth = 1.0f,
};

constexpr inline vk::PipelineMultisampleStateCreateInfo kMultisampleOneSample{
	.rasterizationSamples = vk::SampleCountFlagBits::e1,
};

constexpr inline vk::PipelineDepthStencilStateCreateInfo kNoDepthNoStencil{};
constexpr inline vk::PipelineDepthStencilStateCreateInfo kDepthNoStencil{
	.depthTestEnable       = vk::True,
	.depthWriteEnable      = vk::True,
	.depthCompareOp        = vk::CompareOp::eLess,
	.depthBoundsTestEnable = vk::False,
	.stencilTestEnable     = vk::False,
	.front                 = {},
	.back                  = {},
	.minDepthBounds        = 0.0f,
	.maxDepthBounds        = 1.0f,
};


constexpr inline vk::PipelineColorBlendAttachmentState kBlendAttachment{
	.blendEnable		 = false,
	.srcColorBlendFactor = vk::BlendFactor::eOne,
	.dstColorBlendFactor = vk::BlendFactor::eZero,
	.colorBlendOp		 = vk::BlendOp::eAdd,
	.srcAlphaBlendFactor = vk::BlendFactor::eOne,
	.dstAlphaBlendFactor = vk::BlendFactor::eZero,
	.colorWriteMask = vk::ColorComponentFlagBits::eR
				| vk::ColorComponentFlagBits::eG
				| vk::ColorComponentFlagBits::eB
				| vk::ColorComponentFlagBits::eA,
};

constexpr inline vk::PipelineColorBlendAttachmentState kBlendAttachments[] = {kBlendAttachment};

constexpr inline vk::PipelineColorBlendStateCreateInfo kBlend{
	.logicOpEnable	 = vk::False,
	.logicOp		 = vk::LogicOp::eCopy,
	.blendConstants = {{0.0f, 0.0f, 0.0f, 0.0f}},
};

// constexpr inline u32 kUseMaxPhysicalDevicePushConstantsSize = ~0u;
// constexpr inline vk::PushConstantRange kPushConstantMaxSizeAllStages = {
// 	.stageFlags = vk::ShaderStageFlagBits::eAll,
// 	.offset     = 0,
// 	.size       = kUseMaxPhysicalDevicePushConstantsSize,
// };
// constexpr inline vk::PushConstantRange kOnePushConstantMaxSizeAllStages[] = {kPushConstantMaxSizeAllStages};

constexpr inline vk::DynamicState kDynamicStates[] = {vk::DynamicState::eViewport,
																	vk::DynamicState::eScissor};
} // namespace defaults
} // namespace VB_NAMESPACE
