#ifndef VULKAN_BACKEND_RESOURCE_DEVICE_HPP_
#define VULKAN_BACKEND_RESOURCE_DEVICE_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <set>
#include <string_view>
#else
import std;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include <vulkan/vulkan.h>

#ifndef VB_DEV
#if !defined(VB_VMA_IMPLEMENTATION) || VB_VMA_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#endif
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif //VB_DEV

#include <vulkan_backend/interface/device.hpp>
#include <vulkan_backend/interface/instance.hpp>
#include <vulkan_backend/interface/pipeline.hpp>
#include <vulkan_backend/interface/image.hpp>
#include <vulkan_backend/interface/buffer.hpp>
#include <vulkan_backend/interface/descriptor.hpp>

#include "base.hpp"
#include "instance.hpp"
#include "physical_device.hpp"
#include "../util.hpp"

namespace VB_NAMESPACE {
static inline auto HashCombine(size_t hash, size_t x) -> size_t {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return hash ^ x + 0x9e3779b9 + (hash << 6) + (hash >> 2);
}

static inline auto operator==(Pipeline::Stage const& lhs, Pipeline::Stage const& rhs) -> bool {
	return lhs.stage == rhs.stage && lhs.source.data == rhs.source.data &&
		lhs.entry_point == rhs.entry_point && lhs.compile_options == rhs.compile_options;
}

struct DeviceResource : ResourceBase<InstanceResource>, std::enable_shared_from_this<DeviceResource> {
	VkDevice         handle              = VK_NULL_HANDLE;
	VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;
	VkPipelineCache  pipelineCache       = VK_NULL_HANDLE;

	PhysicalDeviceResource* physicalDevice = nullptr;

	Descriptor descriptor = {};

	// All user-created resources owned by handle classes with
	// std::shared_ptr<...Resource> resource member
	std::set<ResourceBase<DeviceResource>*> resources;

	// Created with device and not changed
	std::vector<QueueResource> queuesResources;
	std::vector<Queue>         queues;
	DeviceInfo                 init_info;

	VmaAllocator vmaAllocator;

	struct SamplerHash {
		auto operator()(SamplerInfo const& desc) const -> size_t {
			size_t hash = 0;
			hash = HashCombine(hash, static_cast<size_t>(desc.magFilter));
			hash = HashCombine(hash, static_cast<size_t>(desc.minFilter));
			hash = HashCombine(hash, static_cast<size_t>(desc.mipmapMode));
			hash = HashCombine(hash, static_cast<size_t>(desc.wrapU));
			hash = HashCombine(hash, static_cast<size_t>(desc.wrapV));
			hash = HashCombine(hash, static_cast<size_t>(desc.wrapW));
			hash = HashCombine(hash, std::hash<float>()(desc.mipLodBias));
			hash = HashCombine(hash, std::hash<bool>()(desc.anisotropyEnable));
			hash = HashCombine(hash, std::hash<float>()(desc.maxAnisotropy));
			hash = HashCombine(hash, std::hash<bool>()(desc.compareEnable));
			hash = HashCombine(hash, static_cast<size_t>(desc.compareOp));
			hash = HashCombine(hash, std::hash<float>()(desc.minLod));
			hash = HashCombine(hash, std::hash<float>()(desc.maxLod));
			hash = HashCombine(hash, static_cast<size_t>(desc.borderColor));
			hash = HashCombine(hash, std::hash<bool>()(desc.unnormalizedCoordinates));
			return hash;
		}

		auto operator()(SamplerInfo const& lhs, SamplerInfo const& rhs) const -> bool {
			return std::memcmp(&lhs, &rhs, sizeof(SamplerInfo)) == 0;
		}
	};

	std::unordered_map<SamplerInfo, VkSampler, SamplerHash, SamplerHash> samplers;

	struct PipelineLibrary {
		struct VertexInputInfo {
			std::span<vk::Format const> vertexAttributes;
			bool lineTopology = false;
		};

		struct PreRasterizationInfo {
			VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
			bool lineTopology = false;
			CullModeFlags cullMode = CullMode::eNone;
			std::span<Pipeline::Stage const> stages;
		};

		struct FragmentShaderInfo {
			VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
			SampleCount samples;
			std::span<Pipeline::Stage const> stages;
		};

		struct FragmentOutputInfo {
			VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
			std::span<vk::Format const> colorFormats;
			bool useDepth;
			vk::Format depthFormat;
			SampleCount samples;
		};

		static inline auto FormatSpanHash(std::span<vk::Format const> formats) -> size_t {
			size_t hash = 0;
			for (vk::Format format: formats) {
				hash = HashCombine(hash, static_cast<size_t>(format));
			}
			return hash;
		}
		
		static inline auto StageHash(Pipeline::Stage const& stage) -> size_t {
			size_t hash = 0;
			hash = HashCombine(hash, static_cast<size_t>(stage.stage));
			hash = HashCombine(hash, std::hash<std::string_view>{}(stage.source.data));
			hash = HashCombine(hash, std::hash<std::string_view>{}(stage.entry_point));
			hash = HashCombine(hash, std::hash<std::string_view>{}(stage.compile_options));
			return hash;
		}

		struct VertexInputHash {
			auto operator()(VertexInputInfo const& info) const -> size_t {
				return HashCombine(FormatSpanHash(info.vertexAttributes), info.lineTopology);
			}

			auto operator()(VertexInputInfo const& a, VertexInputInfo const& b) const -> bool {
				auto span_compare = [](auto const& a, auto const& b) -> bool {
					return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
				};
				return span_compare(a.vertexAttributes, b.vertexAttributes) && a.lineTopology == b.lineTopology;
			}
		};

		struct PreRasterizationHash {
			auto operator()(PreRasterizationInfo const& info) const -> size_t {
				size_t seed = 0;
				for (Pipeline::Stage const& stage: info.stages) {
					seed = HashCombine(seed, StageHash(stage));
				}
				seed = HashCombine(seed, info.lineTopology);
				seed = HashCombine(seed, CullModeFlags::MaskType(info.cullMode));
				return seed;
			}

			auto operator()(PreRasterizationInfo const& a, PreRasterizationInfo const& b) const -> bool {
				return a.stages.size() == b.stages.size() &&
					std::equal(a.stages.begin(), a.stages.end(), b.stages.begin()) && 
					a.lineTopology == b.lineTopology && a.cullMode == b.cullMode;
			}
		};

		struct FragmentOutputHash {
			auto operator()(FragmentOutputInfo const& info) const -> size_t {
				size_t hash = HashCombine(FormatSpanHash(info.colorFormats), reinterpret_cast<size_t>(info.pipelineLayout));
				hash = HashCombine(hash, info.useDepth);
				hash = HashCombine(hash, static_cast<size_t>(info.depthFormat));
				hash = HashCombine(hash, static_cast<size_t>(info.samples));
				return hash;
			}

			auto operator()(FragmentOutputInfo const& a, FragmentOutputInfo const& b) const -> bool {
				return std::equal(a.colorFormats.begin(), a.colorFormats.end(), b.colorFormats.begin()) && 
					a.pipelineLayout == b.pipelineLayout && 
					a.useDepth == b.useDepth && 
					a.depthFormat == b.depthFormat && 
					a.samples == b.samples;
			}
		};

		struct FragmentShaderHash {
			auto operator()(FragmentShaderInfo const& info) const -> size_t {
				size_t hash = 0;
				for (auto const& stage: info.stages) {
					hash = HashCombine(hash, StageHash(stage));
				}
				hash = HashCombine(hash, reinterpret_cast<size_t>(info.pipelineLayout));
				hash = HashCombine(hash, static_cast<size_t>(info.samples));
				return hash;
			}
			auto operator()(FragmentShaderInfo const& a, FragmentShaderInfo const& b) const -> bool {
				return a.stages.size() == b.stages.size() && 
					std::equal(a.stages.begin(), a.stages.end(), b.stages.begin()) && 
					a.pipelineLayout == b.pipelineLayout && 
					a.samples == b.samples;
			}
		};

		DeviceResource* device = nullptr;
		std::unordered_map<VertexInputInfo, VkPipeline,VertexInputHash, VertexInputHash> vertexInputInterfaces;
		std::unordered_map<PreRasterizationInfo, VkPipeline, PreRasterizationHash, PreRasterizationHash> preRasterizationShaders;
		std::unordered_map<FragmentOutputInfo, VkPipeline, FragmentOutputHash, FragmentOutputHash> fragmentOutputInterfaces;
		std::unordered_map<FragmentShaderInfo, VkPipeline, FragmentShaderHash, FragmentShaderHash> fragmentShaders;

		Pipeline CreatePipeline(PipelineInfo const& desc);
	private:
		void CreateVertexInputInterface(VertexInputInfo const& info);
		void CreatePreRasterizationShaders(PreRasterizationInfo const& info);
		void CreateFragmentShader(FragmentShaderInfo const& info);
		void CreateFragmentOutputInterface(FragmentOutputInfo const& info);
		auto LinkPipeline(std::array<VkPipeline, 4> const& pipelines, VkPipelineLayout layout) -> VkPipeline;

	} pipelineLibrary;

	u8* stagingCpu = nullptr;
	u32 stagingOffset = 0;
	Buffer staging;
	
	std::vector<char const*> requiredExtensions = { // Physical Device Extensions
		// VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		// VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		// VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		// VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		// VK_KHR_RAY_QUERY_EXTENSION_NAME,
		// VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME,
	};

	void Create(DeviceInfo const& info);

	auto CreateBuffer(BufferInfo const& info) -> Buffer;
	auto CreateImage(ImageInfo const& info)   -> Image;
	auto CreatePipeline(PipelineInfo const& info)         -> Pipeline;
	auto CreateSampler(SamplerInfo const& info)           -> VkSampler;
	auto GetOrCreateSampler(SamplerInfo const& info = {}) -> VkSampler;

	void SetDebugUtilsObjectName(VkDebugUtilsObjectNameInfoEXT const& pNameInfo);

	PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT = nullptr;
	// PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
	// PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
	// PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
	// PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
	// PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
	// PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;

	using ResourceBase::ResourceBase;

	inline auto GetInstance() -> InstanceResource* { return owner; }

	auto GetName() -> char const* { 
		return physicalDevice->physicalProperties2.properties.deviceName;
	}

	auto GetType() -> char const* override { 
		return "DeviceResource";
	}

	void Free() override {
		VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
		vkDeviceWaitIdle(handle);

		if (imguiDescriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(handle, imguiDescriptorPool, owner->allocator);
		}

		for (auto& [_, pipeline]: pipelineLibrary.vertexInputInterfaces) {
			vkDestroyPipeline(handle, pipeline, owner->allocator);
		}
		for (auto& [_, pipeline]: pipelineLibrary.preRasterizationShaders) {
			vkDestroyPipeline(handle, pipeline, owner->allocator);
		}
		for (auto& [_, pipeline]: pipelineLibrary.fragmentOutputInterfaces) {
			vkDestroyPipeline(handle, pipeline, owner->allocator);
		}
		for (auto& [_, pipeline]: pipelineLibrary.fragmentShaders) {
			vkDestroyPipeline(handle, pipeline, owner->allocator);
		}

		pipelineLibrary.vertexInputInterfaces.clear();
		pipelineLibrary.preRasterizationShaders.clear();
		pipelineLibrary.fragmentOutputInterfaces.clear();
		pipelineLibrary.fragmentShaders.clear();

		VB_LOG_TRACE("[ Free ] Cleaning up %zu device resources...", resources.size());

		std::erase_if(resources, [](ResourceBase<DeviceResource>* const res) {
			res->Free();
			return 1;
		});

		for (auto& [_, sampler] : samplers) {
			vkDestroySampler(handle, sampler, owner->allocator);
		}

		vmaDestroyAllocator(vmaAllocator);

		vkDestroyDevice(handle, owner->allocator);
		VB_LOG_TRACE("[ Free ] Destroyed logical device");
		handle = VK_NULL_HANDLE;
	}
};

} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_RESOURCE_DEVICE_HPP_