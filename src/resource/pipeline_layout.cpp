#ifndef VB_USE_STD_MODULE
#include <utility>
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/interface/device/device.hpp"
#include "vulkan_backend/interface/pipeline_layout/pipeline_layout.hpp"
#include "vulkan_backend/log.hpp"
#include "vulkan_backend/vk_result.hpp"

namespace VB_NAMESPACE {

PipelineLayout::PipelineLayout(Device& device, vk::PipelineLayout pipeline_layout)
	: vk::PipelineLayout(pipeline_layout), ResourceBase(&device) {}

PipelineLayout::PipelineLayout(PipelineLayout&& other)
	: vk::PipelineLayout(std::exchange(static_cast<vk::PipelineLayout&>(other), vk::PipelineLayout{})),
	  ResourceBase(std::move(other)) {}

PipelineLayout::PipelineLayout(Device& device, PipelineLayoutInfo const& info) {
	Create(device, info);
}

PipelineLayout& PipelineLayout::operator=(PipelineLayout&& other) {
	vk::PipelineLayout::operator=(
		std::exchange(static_cast<vk::PipelineLayout&>(other), vk::PipelineLayout{}));
	ResourceBase::operator=(std::move(other));
	return *this;
}

auto PipelineLayout::GetResourceTypeName() const -> char const* { return "PipelineLayoutResource"; }

PipelineLayout::~PipelineLayout() { Free(); }

auto PipelineLayout::Create(Device& device, PipelineLayoutInfo const& info) -> vk::Result {
	ResourceBase::SetOwner(&device);
	vk::PipelineLayoutCreateInfo pipeline_layout_info{
		.setLayoutCount         = static_cast<u32>(info.descriptor_set_layouts.size()),
		.pSetLayouts            = info.descriptor_set_layouts.data(),
		.pushConstantRangeCount = static_cast<u32>(info.push_constant_ranges.size()),
		.pPushConstantRanges    = info.push_constant_ranges.data(),
	};

	VB_VERIFY_VK_RESULT(device.createPipelineLayout(&pipeline_layout_info, device.GetAllocator(), this),
						info.check_vk_results, "Failed to create pipeline layout!", {});
	return vk::Result::eSuccess;
}

void PipelineLayout::Free() {
	if (!vk::PipelineLayout::operator bool())
		return;
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), GetResourceTypeName());
	GetDevice().destroyPipelineLayout(*this, GetDevice().GetAllocator());
	vk::PipelineLayout::operator=(vk::PipelineLayout{});
}
}; // namespace VB_NAMESPACE