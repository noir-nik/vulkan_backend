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

#include "vulkan_backend/interface/descriptor.hpp"
#include "vulkan_backend/interface/device.hpp"
#include "vulkan_backend/interface/instance.hpp"
#include "vulkan_backend/log.hpp"
#include "vulkan_backend/macros.hpp"
#include "vulkan_backend/vk_result.hpp"


namespace VB_NAMESPACE {
auto BindlessDescriptor::GetBindingInfo(u32 binding) const
	-> vk::DescriptorSetLayoutBinding const& {
	auto it = bindings.find(binding);
	VB_ASSERT(it != bindings.end(), "Descriptor binding not found");
	return it->second;
}

auto BindlessDescriptor::PopID(u32 binding) -> u32 {
	auto it = bindings.find(binding);
	VB_ASSERT(it != bindings.end(), "Descriptor binding not found");
	u32 id = it->second.resource_ids.back();
	it->second.resource_ids.pop_back();
	return id;
}

void BindlessDescriptor::PushID(u32 binding, u32 id) {
	auto it = bindings.find(binding);
	VB_ASSERT(it != bindings.end(), "Descriptor binding not found");
	it->second.resource_ids.push_back(id);
}

void Descriptor::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetResourceTypeName(), "Descriptor");
	if (owner) {
		device->destroyDescriptorPool(pool, device->GetAllocator());
		device->destroyDescriptorSetLayout(layout);
	}
}

Descriptor::Descriptor(std::shared_ptr<Device> const& device, DescriptorInfo const& info)
	: ResourceBase(device) {
	Create(device.get(), info);
}

Descriptor::Descriptor(std::shared_ptr<Device> const& device, vk::DescriptorPool pool,
					   vk::DescriptorSetLayout layout, vk::DescriptorSet set)
	: ResourceBase(device), pool(pool), layout(layout), set(set), device(device.get()) {}

Descriptor::Descriptor(Descriptor&& other) noexcept
	: pool(std::exchange(other.pool, nullptr)), layout(std::exchange(other.layout, nullptr)),
	  set(std::exchange(other.set, nullptr)) {}
Descriptor& Descriptor::operator=(Descriptor&& other) noexcept {
	if (this != &other) {
		pool   = std::exchange(other.pool, nullptr);
		layout = std::exchange(other.layout, nullptr);
		set	   = std::exchange(other.set, nullptr);
	}
	return *this;
}

Descriptor::~Descriptor() { Free(); }

void Descriptor::Create(Device* device, DescriptorInfo const& info) {
	this->device = device;
	this->pool	 = CreateDescriptorPool(info);
	this->layout = CreateDescriptorSetLayout(info);
	this->set	 = CreateDescriptorSets(info);
}

BindlessDescriptor::BindlessDescriptor(std::shared_ptr<Device> const& device, DescriptorInfo const& info) {
	Create(device.get(), info);
}

void BindlessDescriptor::Create(Device* device, DescriptorInfo const& info) {
	Descriptor::Create(device, info);
	bindings.reserve(info.bindings.size());
	for (auto const& binding : info.bindings) {
		auto [it, success] = bindings.try_emplace(binding.binding, binding);
		VB_ASSERT(success, "Duplicate binding found in descriptor binding list");
		auto& vec = it->second.resource_ids;
		vec.resize(binding.descriptorCount);
		std::iota(vec.begin(), vec.end(), 0);
	}
}

auto Descriptor::CreateDescriptorPool(DescriptorInfo const& info) -> vk::DescriptorPool {
	VB_VLA(vk::DescriptorPoolSize, pool_sizes, info.bindings.size());
	for (u32 i = 0; auto const& binding_info : info.bindings) {
		pool_sizes[i] = {static_cast<vk::DescriptorType>(binding_info.descriptorType),
						 binding_info.descriptorCount};
		++i;
	}

	vk::DescriptorPoolCreateInfo poolInfo = {
		.flags		   = info.pool_flags,
		.maxSets	   = 1,
		.poolSizeCount = static_cast<u32>(pool_sizes.size()),
		.pPoolSizes	   = pool_sizes.data(),
	};

	// std::tie(result, pool) = device->createDescriptorPool(poolInfo,
	// device->GetAllocator());
	auto [result, descriptorPool] =
		device->createDescriptorPool(poolInfo, device->GetAllocator());
	VB_CHECK_VK_RESULT(result, "Failed to create descriptor pool!");
	return descriptorPool;
}

auto Descriptor::CreateDescriptorSetLayout(DescriptorInfo const& info) -> vk::DescriptorSetLayout {
	// fill unspecified flags with zero
	VB_VLA(vk::DescriptorBindingFlags, binding_flags, info.bindings.size());
	std::fill(binding_flags.begin(), binding_flags.end(), vk::DescriptorBindingFlags{});
	for (u32 i = 0; auto const& flags : info.binding_flags) {
		binding_flags[i] = flags;
		++i;
	}

	vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {
		.bindingCount  = static_cast<u32>(info.bindings.size()),
		.pBindingFlags = binding_flags.data(),
	};

	vk::DescriptorSetLayoutCreateInfo layoutInfo = {
		.pNext		  = &bindingFlagsInfo,
		.flags		  = info.layout_flags,
		.bindingCount = static_cast<u32>(info.bindings.size()),
		.pBindings	  = info.bindings.data(),
	};

	auto [result, descriptorSetLayout] = device->createDescriptorSetLayout(layoutInfo, nullptr);
	VB_CHECK_VK_RESULT(result, "Failed to create descriptor set layout!");
	return descriptorSetLayout;
}

auto Descriptor::CreateDescriptorSets(DescriptorInfo const& info) -> vk::DescriptorSet {
	vk::DescriptorSetAllocateInfo setInfo{
		.descriptorPool		= pool,
		.descriptorSetCount = 1,
		.pSetLayouts		= &layout,
	};

	// auto [result, descriptorSet] = device->allocateDescriptorSets(setInfo);
	vk::DescriptorSet descriptorSet;
	VB_VK_RESULT	  result = device->allocateDescriptorSets(&setInfo, &descriptorSet);
	VB_CHECK_VK_RESULT(result, "Failed to allocate descriptor set!");
	return descriptorSet;
}

}; // namespace VB_NAMESPACE