#pragma once

#ifndef VB_USE_STD_MODULE
#include <unordered_map>
#elif defined(VB_DEV)
import std;
#endif

#include "vulkan_backend/classes/base.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/descriptor/info.hpp"
#include "vulkan_backend/types.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
class Descriptor : public ResourceBase<Device> {
  public:
	Descriptor(Device& device, DescriptorInfo const& info);
	Descriptor(Device& device, vk::DescriptorPool pool, vk::DescriptorSetLayout layout,
			   vk::DescriptorSet set);
	Descriptor(Descriptor&& other) noexcept;
	Descriptor& operator=(Descriptor&& other) noexcept;
	// Frees all resources
	~Descriptor();
	// Todo: add move constructor and move assignment
	vk::DescriptorPool		pool   = nullptr;
	vk::DescriptorSetLayout layout = nullptr;
	vk::DescriptorSet		set	   = nullptr;
	inline auto GetDevice() const -> Device& { return *owner; }
  private:
	Descriptor() = default;
	void Create(Device& device, DescriptorInfo const& info);
	void Free();
	void SetDebugUtilsNames();
	auto CreateDescriptorSetLayout(DescriptorInfo const& info) -> vk::DescriptorSetLayout;
	auto CreateDescriptorPool(DescriptorInfo const& info) -> vk::DescriptorPool;
	auto CreateDescriptorSets(DescriptorInfo const& info) -> vk::DescriptorSet;

	// Bindless Resource IDs for descriptor indexing
	friend PipelineLibrary;
	friend Device;
	friend Buffer;
	friend Image;
};

class BindlessDescriptor : public Descriptor {
  public:
	BindlessDescriptor(Device& device, DescriptorInfo const& info);
	// Get binding of bindless array with respective type
	auto GetBindingInfo(u32 binding) const -> vk::DescriptorSetLayoutBinding const&;
	void SetDebugUtilsNames();
	auto PopID(u32 binding) -> u32;
	void PushID(u32 binding, u32 id);
  private:
	friend Device;
	void Create(Device& device, DescriptorInfo const& info);
	struct BindingInfoInternal : vk::DescriptorSetLayoutBinding {
		std::vector<u32> resource_ids;
	};

	std::unordered_map<u32, BindingInfoInternal> bindings;
};

} // namespace VB_NAMESPACE
