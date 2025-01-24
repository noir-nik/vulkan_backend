#pragma once

#ifndef VB_USE_STD_MODULE
#include <memory>
#include <unordered_map>
#elif defined(VB_DEV)
import std;
#endif

#include "vulkan_backend/classes/base.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/info/descriptor.hpp"
#include "vulkan_backend/types.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
class Descriptor : public ResourceBase<Device> {
  public:
	Descriptor(std::shared_ptr<Device> const& device, DescriptorInfo const& info);
	Descriptor(std::shared_ptr<Device> const& device, vk::DescriptorPool pool, vk::DescriptorSetLayout layout,
			   vk::DescriptorSet set);
	Descriptor(Descriptor&& other) noexcept;
	Descriptor& operator=(Descriptor&& other) noexcept;
	// Frees all resources
	~Descriptor();
	// Todo: add move constructor and move assignment
	vk::DescriptorPool		pool   = nullptr;
	vk::DescriptorSetLayout layout = nullptr;
	vk::DescriptorSet		set	   = nullptr;

  protected:
	Descriptor() = default;
	void Create(Device* device, DescriptorInfo const& info);
  private:
  	// For being device member;
	Device* device = nullptr;
	void Free();
	inline auto GetDevice() const -> Device* { return owner.get(); }
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
	BindlessDescriptor(std::shared_ptr<Device> const& device, DescriptorInfo const& info);
	// Get binding of bindless array with respective type
	auto GetBindingInfo(u32 binding) const -> vk::DescriptorSetLayoutBinding const&;
	auto PopID(u32 binding) -> u32;
	void PushID(u32 binding, u32 id);
  private:
	friend Device;
	BindlessDescriptor() = default;
	void Create(Device* device, DescriptorInfo const& info);
	struct BindingInfoInternal : vk::DescriptorSetLayoutBinding {
		std::vector<u32> resource_ids;
	};

	std::unordered_map<u32, BindingInfoInternal> bindings;
};

} // namespace VB_NAMESPACE
