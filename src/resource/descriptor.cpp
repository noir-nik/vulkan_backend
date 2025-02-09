#ifndef VB_USE_STD_MODULE
#include <numeric>
#include <span>
#include <utility>
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/interface/descriptor/descriptor.hpp"
#include "vulkan_backend/interface/device/device.hpp"
#include "vulkan_backend/interface/instance/instance.hpp"
#include "vulkan_backend/log.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/util/enumerate.hpp"
#include "vulkan_backend/vk_result.hpp"

namespace VB_NAMESPACE {

Descriptor::Descriptor(Device& device, DescriptorInfo const& info) { Create(device, info); }

Descriptor::Descriptor(Device& device, vk::DescriptorPool pool, vk::DescriptorSetLayout layout, vk::DescriptorSet set)
	: ResourceBase(&device), pool(pool), layout(layout), set(set) {}

Descriptor::Descriptor(Descriptor&& other) noexcept
	: pool(std::exchange(other.pool, nullptr)), layout(std::exchange(other.layout, nullptr)),
	  set(std::exchange(other.set, nullptr)) {}

Descriptor& Descriptor::operator=(Descriptor&& other) noexcept {
	ResourceBase::operator=(std::move(other));
	pool   = std::exchange(other.pool, nullptr);
	layout = std::exchange(other.layout, nullptr);
	set    = std::exchange(other.set, nullptr);
	return *this;
}

Descriptor::~Descriptor() { Free(); }

auto Descriptor::Create(Device& device, DescriptorInfo const& info) -> vk::Result {
	VB_ASSERT(!info.bindings.empty(), "Descriptor info must have at least one binding");
	ResourceBase::SetOwner(&device);
	VB_VERIFY_VK_RESULT(CreateDescriptorPool(info), info.check_vk_results, "Failed to create descriptor pool!", {});
	VB_VERIFY_VK_RESULT(CreateDescriptorSetLayout(info), info.check_vk_results, "Failed to create descriptor set layout!", {
		GetDevice().destroyDescriptorPool(pool, GetDevice().GetAllocator());
		pool = nullptr;
	});
	VB_VERIFY_VK_RESULT(CreateDescriptorSets(info), info.check_vk_results, "Failed to allocate descriptor set!", {
		GetDevice().destroyDescriptorPool(pool, GetDevice().GetAllocator());
		GetDevice().destroyDescriptorSetLayout(layout);
		pool   = nullptr;
		layout = nullptr;
		set    = nullptr;
	});
	return vk::Result::eSuccess;
}

void Descriptor::Free() {
	if (pool == nullptr)
		return;
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), "Descriptor");
	GetDevice().destroyDescriptorPool(pool, GetDevice().GetAllocator());
	GetDevice().destroyDescriptorSetLayout(layout);
	pool   = nullptr;
	layout = nullptr;
	set    = nullptr;
}

void Descriptor::SetDebugUtilsNames() {
	GetDevice().SetDebugUtilsName(vk::ObjectType::eDescriptorSet, &set, "DescriptorSet");
	GetDevice().SetDebugUtilsName(vk::ObjectType::eDescriptorPool, &pool, "DescriptorPool");
	GetDevice().SetDebugUtilsName(vk::ObjectType::eDescriptorSetLayout, &layout, "DescriptorSetLayout");
}

auto BindlessDescriptor::GetBindingInfo(u32 binding) const -> vk::DescriptorSetLayoutBinding const& {
	auto it = bindings.find(binding);
	VB_ASSERT(it != bindings.end(), "Descriptor binding not found");
	return it->second;
}

auto Descriptor::CreateDescriptorPool(DescriptorInfo const& info) -> vk::Result {
	VB_VLA(vk::DescriptorPoolSize, pool_sizes, info.bindings.size());
	for (auto [i, binding_info] : util::enumerate(info.bindings)) {
		pool_sizes[i] = {static_cast<vk::DescriptorType>(binding_info.descriptorType), binding_info.descriptorCount};
	}
	vk::DescriptorPoolCreateInfo poolInfo = {
		.flags         = info.pool_flags,
		.maxSets       = 1,
		.poolSizeCount = static_cast<u32>(pool_sizes.size()),
		.pPoolSizes    = pool_sizes.data(),
	};

	return GetDevice().createDescriptorPool(&poolInfo, GetDevice().GetAllocator(), &pool);
}

auto Descriptor::CreateDescriptorSetLayout(DescriptorInfo const& info) -> vk::Result {
	// fill unspecified flags with zero
	VB_VLA(vk::DescriptorBindingFlags, binding_flags, info.bindings.size());
	auto end = std::copy_n(info.binding_flags.begin(), info.binding_flags.size(), binding_flags.begin());
	std::fill(end, binding_flags.end(), vk::DescriptorBindingFlags{});

	vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {
		.bindingCount  = static_cast<u32>(info.bindings.size()),
		.pBindingFlags = binding_flags.data(),
	};

	vk::DescriptorSetLayoutCreateInfo layoutInfo = {
		.pNext        = &bindingFlagsInfo,
		.flags        = info.layout_flags,
		.bindingCount = static_cast<u32>(info.bindings.size()),
		.pBindings    = info.bindings.data(),
	};

	return GetDevice().createDescriptorSetLayout(&layoutInfo, GetDevice().GetAllocator(), &layout);
}

auto Descriptor::CreateDescriptorSets(DescriptorInfo const& info) -> vk::Result {
	vk::DescriptorSetAllocateInfo setInfo{
		.descriptorPool     = pool,
		.descriptorSetCount = 1,
		.pSetLayouts        = &layout,
	};

	return GetDevice().allocateDescriptorSets(&setInfo, &set);
}

BindlessDescriptor::BindlessDescriptor(Device& device, DescriptorInfo const& info) : Descriptor(device, info) {
	Create(device, info);
}

BindlessDescriptor::BindlessDescriptor(BindlessDescriptor&& other)
	: Descriptor(std::move(other)) {
	bindings = std::move(other.bindings);
}

BindlessDescriptor& BindlessDescriptor::operator=(BindlessDescriptor&& other) {
	if (this != &other) {
		Descriptor::operator=(std::move(other));
		bindings = std::move(other.bindings);
	}
	return *this;
}

BindlessDescriptor::~BindlessDescriptor() { Free(); }

void BindlessDescriptor::Free() {
	Descriptor::Free();
	bindings.clear();
}

void BindlessDescriptor::Create(Device& device, DescriptorInfo const& info) {
	bindings.reserve(info.bindings.size());
	for (auto const& binding : info.bindings) {
		auto [it, success] = bindings.try_emplace(binding.binding, binding);
		VB_ASSERT(success, "Duplicate binding found in descriptor binding list");
		auto& vec = it->second.resource_ids;
		vec.resize(binding.descriptorCount);
		std::iota(vec.rbegin(), vec.rend(), 0);
	}
}

auto BindlessDescriptor::PopID(u32 binding) -> u32 {
	auto it = bindings.find(binding);
	VB_ASSERT(it != bindings.end(), "Descriptor binding not found");
	VB_ASSERT(!it->second.resource_ids.empty(), "BindlessDescriptor::PopID(u32): Descriptor binding is empty");
	u32 id = it->second.resource_ids.back();
	it->second.resource_ids.pop_back();
	return id;
}

void BindlessDescriptor::PushID(u32 binding, u32 id) {
	auto it = bindings.find(binding);
	VB_ASSERT(it != bindings.end(), "Descriptor binding not found");
	it->second.resource_ids.push_back(id);
}

}; // namespace VB_NAMESPACE