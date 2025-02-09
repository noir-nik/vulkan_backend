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
	Descriptor() = default;
	Descriptor(Device& device, DescriptorInfo const& info);
	Descriptor(Device& device, vk::DescriptorPool pool, vk::DescriptorSetLayout layout, vk::DescriptorSet set);
	Descriptor(Descriptor&& other) noexcept;
	Descriptor& operator=(Descriptor&& other) noexcept;

	// Calls Free
	~Descriptor();

	// Create with result checked
	auto Create(Device& device, DescriptorInfo const& info) -> vk::Result;

	// Frees all resources
	void Free();

	inline auto GetPool() const -> vk::DescriptorPool { return pool; }
	inline auto GetLayout() const -> vk::DescriptorSetLayout { return layout; }
	inline auto GetSet() const -> vk::DescriptorSet { return set; }
	inline auto GetDevice() const -> Device& { return *GetOwner(); }

  private:
	void SetDebugUtilsNames();
	auto CreateDescriptorSetLayout(DescriptorInfo const& info) -> vk::Result;
	auto CreateDescriptorPool(DescriptorInfo const& info) -> vk::Result;
	auto CreateDescriptorSets(DescriptorInfo const& info) -> vk::Result;

	vk::DescriptorPool      pool   = nullptr;
	vk::DescriptorSetLayout layout = nullptr;
	vk::DescriptorSet       set    = nullptr;
};

class BindlessDescriptor : public Descriptor {
  public:
	// No-op constructor
	BindlessDescriptor() = default;

	// RAII constructor, calls Create
	BindlessDescriptor(Device& device, DescriptorInfo const& info);

	// Move constructor
	BindlessDescriptor(BindlessDescriptor&& other);

	// Move assignment
	BindlessDescriptor& operator=(BindlessDescriptor&& other);

	// Calls Free
	~BindlessDescriptor();

	// Create Manually
	void Create(Device& device, DescriptorInfo const& info);

	// Manually free resources, safe to call multiple times
	void Free() override;

	// Get binding of bindless array with respective type
	auto GetBindingInfo(u32 binding) const -> vk::DescriptorSetLayoutBinding const&;
	void SetDebugUtilsNames();

	[[nodiscard("Aquired id should be pushed back")]]
	auto PopID(u32 binding) -> u32;
	void PushID(u32 binding, u32 id);

  private:
	friend Device;
	struct BindingInfoInternal : vk::DescriptorSetLayoutBinding {
		std::vector<u32> resource_ids;
	};

	// Bindless Resource IDs for descriptor indexing
	std::unordered_map<u32, BindingInfoInternal> bindings;
};

} // namespace VB_NAMESPACE
