#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <span>
#include <numeric>
#else
import std;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "descriptor.hpp"
#include "instance.hpp"
#include "device.hpp"
namespace VB_NAMESPACE {

auto Descriptor::GetBinding(DescriptorType type) -> u32 {
	return resource->GetBinding(type);
}

auto DescriptorResource::GetType() const -> char const* { return "DescriptorResource"; }

auto DescriptorResource::GetInstance() const -> InstanceResource* { return owner->owner; }

void DescriptorResource::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
	vkDestroyDescriptorPool(owner->handle, pool, owner->owner->allocator);
	vkDestroyDescriptorSetLayout(owner->handle, layout, owner->owner->allocator);
}

void DescriptorResource::Create(std::span<BindingInfo const> binding_infos) {
	std::set<u32> specified_bindings;
	bindings.reserve(binding_infos.size());
	for (auto const& info : binding_infos) { // todo: allow duplicate DescriptorType
		auto [_, success] = bindings.try_emplace(info.type, info);
		VB_ASSERT(success, "Duplicate DescriptorType in descriptor binding list");
	}
	for (auto const& [_, info] : bindings) {
		if (info.binding != BindingInfo::kBindingAuto) {
			if (!specified_bindings.insert(info.binding).second) {
				VB_ASSERT(false, "Duplicate binding in descriptor binding list");
			}
		}
	}
	u32 next_binding = 0;
	for (auto& [_, info] : bindings) {
		if (info.binding == BindingInfo::kBindingAuto) {
			while (specified_bindings.contains(next_binding)) {
				++next_binding;
			}
			info.binding = next_binding;
			// specified_bindings.insert(info.binding);
			++next_binding;
		}
	}

	VB_LOG_TRACE("[ Descriptor ] Selected bindings:");
	for (auto const& [type, info] : bindings) {
		VB_LOG_TRACE("%s: %u", vk::to_string(type).data(), info.binding);
	}
	// fill with reversed indices so that 0 is at the back
	for (auto const& binding : binding_infos) {
		auto it = bindings.find(binding.type);
		auto& vec = it->second.resourceIDs;
		vec.resize(binding.count);
		std::iota(vec.rbegin(), vec.rend(), 0);
	}
	this->pool   = CreateDescriptorPool();
	this->layout = CreateDescriptorSetLayout();
	this->set    = CreateDescriptorSets();
}

auto DescriptorResource::CreateDescriptorPool() -> VkDescriptorPool {
	VB_VLA(VkDescriptorPoolSize, pool_sizes, bindings.size());
	for (u32 i = 0; auto const& [type, binding] : bindings) {
		pool_sizes[i] = {static_cast<VkDescriptorType>(type), binding.count};
		++i;
	}
	VkDescriptorPoolCreateFlags pool_flags = std::any_of(bindings.begin(), bindings.end(),
			[](auto const &binding) { return binding.second.update_after_bind; })
		? VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
		: 0;

	VkDescriptorPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = pool_flags,
		.maxSets = 1,
		.poolSizeCount = static_cast<u32>(pool_sizes.size()),
		.pPoolSizes = pool_sizes.data(),
	};

	VkDescriptorPool descriptorPool;
	VB_VK_RESULT result = vkCreateDescriptorPool(owner->handle, &poolInfo, nullptr, &descriptorPool);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create descriptor pool!");
	return descriptorPool;
}

auto DescriptorResource::CreateDescriptorSetLayout() -> VkDescriptorSetLayout {
	VB_VLA(VkDescriptorSetLayoutBinding, layout_bindings, bindings.size());
	VB_VLA(VkDescriptorBindingFlags, binding_flags, bindings.size());

	for (u32 i = 0; auto const& [type, binding] : bindings) {
		layout_bindings[i] = {
			.binding = binding.binding,
			.descriptorType = static_cast<VkDescriptorType>(type),
			.descriptorCount = binding.count,
			.stageFlags = static_cast<VkShaderStageFlags>(binding.stage_flags),
		};
		binding_flags[i]  = binding.update_after_bind ? VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT : 0;
		binding_flags[i] |= binding.partially_bound   ? VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT   : 0;
		++i;
	}

	VkDescriptorBindingFlags layout_flags = std::any_of(bindings.cbegin(), bindings.cend(),
			[](auto const &info) { return info.second.update_after_bind; })
		? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT
		: 0;

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindingFlags = binding_flags.data(),
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = &bindingFlagsInfo,
		.flags = layout_flags,
		.bindingCount = static_cast<u32>(layout_bindings.size()),
		.pBindings = layout_bindings.data(),
	};

	VkDescriptorSetLayout descriptorSetLayout;
	VB_VK_RESULT result = vkCreateDescriptorSetLayout(owner->handle, &layoutInfo, nullptr, &descriptorSetLayout);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to allocate descriptor set!");
	return descriptorSetLayout;
}

auto DescriptorResource::CreateDescriptorSets() -> VkDescriptorSet {
	VkDescriptorSetAllocateInfo setInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};
	VkDescriptorSet descriptorSet;
	VB_VK_RESULT result = vkAllocateDescriptorSets(owner->handle, &setInfo, &descriptorSet);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to allocate descriptor set!");
	return descriptorSet;
}

}; // namespace VB_NAMESPACE