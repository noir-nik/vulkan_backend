#ifndef VULKAN_BACKEND_RESOURCE_DESCRIPTOR_HPP_
#define VULKAN_BACKEND_RESOURCE_DESCRIPTOR_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <unordered_map>
#include <vector>
#else
import std;
#endif

#include <vulkan/vulkan.h>

#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/descriptor.hpp"
#include "base.hpp"

namespace VB_NAMESPACE {
struct DescriptorResource : ResourceBase<DeviceResource> {
	VkDescriptorPool      pool   = VK_NULL_HANDLE;
	VkDescriptorSetLayout layout = VK_NULL_HANDLE;
	VkDescriptorSet       set    = VK_NULL_HANDLE;

	// Bindless Resource IDs for descriptor indexing
	struct BindingInfoInternal : BindingInfo {
		std::vector<u32> resourceIDs;
	};
	std::unordered_map<DescriptorType, BindingInfoInternal> bindings;

	using ResourceBase::ResourceBase;

	inline auto PopID(DescriptorType type) -> u32 {
		auto it = bindings.find(type);
		VB_ASSERT(it != bindings.end(), "Descriptor type not found");
		u32 id = it->second.resourceIDs.back();
		it->second.resourceIDs.pop_back();
		return id; 
	}

	inline void PushID(DescriptorType type, u32 id) {
		auto it = bindings.find(type);
		VB_ASSERT(it != bindings.end(), "Descriptor type not found");
		it->second.resourceIDs.push_back(id);
	}

	inline auto GetBinding(DescriptorType type) -> u32 {
		auto it = bindings.find(type);
		VB_ASSERT(it != bindings.end(), "Descriptor type not found");
		return it->second.binding;
	}

	void Create(std::span<BindingInfo const> bindings);
	auto CreateDescriptorSetLayout() -> VkDescriptorSetLayout;
	auto CreateDescriptorPool()      -> VkDescriptorPool;
	auto CreateDescriptorSets()      -> VkDescriptorSet;

	auto GetType() const -> char const* override;
	auto GetInstance() const -> InstanceResource* override;
	void Free() override;
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_RESOURCE_DESCRIPTOR_HPP_