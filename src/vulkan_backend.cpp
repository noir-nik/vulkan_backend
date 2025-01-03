#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <string_view>
#include <filesystem>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <array>
#include <set>
#else
import std;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include <vulkan/vulkan.h>

#ifndef _VB_EXT_MODULES
#if !defined(VB_VMA_IMPLEMENTATION) || VB_VMA_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#endif
#include <vk_mem_alloc.h>
#else
import vk_mem_alloc;
#endif //_VB_EXT_MODULES

#include <vulkan_backend/config.hpp>
#include <vulkan_backend/structs.hpp>
#include <vulkan_backend/core.hpp>
#include <vulkan_backend/util.hpp>

#ifdef MemoryBarrier
#undef MemoryBarrier
#endif

namespace VB_NAMESPACE {

u32 constexpr NullRID = ~0;

LogLevel global_log_level = LogLevel::Trace;

namespace {
	static inline auto HashCombine(size_t hash, size_t x) -> size_t {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return hash ^ x + 0x9e3779b9 + (hash << 6) + (hash >> 2);
};
} // Util

static inline auto operator==(Pipeline::Stage const& lhs, Pipeline::Stage const& rhs) -> bool {
	return lhs.stage == rhs.stage && lhs.source.data == rhs.source.data &&
		lhs.entry_point == rhs.entry_point && lhs.compile_options == rhs.compile_options;
}

struct NoCopy {
	NoCopy() = default;
	NoCopy(const NoCopy&) = delete;
	NoCopy& operator=(const NoCopy&) = delete;
	NoCopy(NoCopy&&) = default;
	NoCopy& operator=(NoCopy&&) = default;
	~NoCopy() = default;
};

struct NoCopyNoMove {
	NoCopyNoMove() = default;
	NoCopyNoMove(const NoCopyNoMove&) = delete;
	NoCopyNoMove& operator=(const NoCopyNoMove&) = delete;
	NoCopyNoMove(NoCopyNoMove&&) = delete;
	NoCopyNoMove& operator=(NoCopyNoMove&&) = delete;
	~NoCopyNoMove() = default;
};

template <typename Owner>
class ResourceBase : NoCopyNoMove {
public:
	Owner* owner;
	std::string name;
	ResourceBase(Owner* owner, std::string_view const name = "") : owner(owner), name(name) {}
	virtual ~ResourceBase() = default;
	virtual void Free() = 0;
	virtual auto GetType() -> char const* = 0;
	auto GetName() -> char const* {return name.data();};
};

template <typename Owner>
struct ResourceDeleter {
	std::weak_ptr<Owner> weak_owner;

	void operator()(ResourceBase<Owner>* ptr) {
		if (auto owner = weak_owner.lock()) {
			ptr->Free();
			owner->resources.erase(ptr);
		}
		delete ptr;
	}
};

// We could use std::allocate_shared to avoid double allocation in std::shared_ptr
// constructor but in that case control block would be in same allocation with Resource
// and memomy would not be freed upon destruction if a single weak_ptr exists
template<typename Res, typename Owner>
requires std::is_base_of_v<std::enable_shared_from_this<Owner>, Owner> && (!std::same_as<Res, Owner>)
auto MakeResource(Owner* owner_ptr, std::string_view const name = "") -> std::shared_ptr<Res> {
	// Create a new resource with custom deleter
	auto res =  std::shared_ptr<Res>(
		new Res(owner_ptr, name),
		ResourceDeleter{owner_ptr->weak_from_this()}
	);
	// Log verbose
	VB_LOG_TRACE("[ MakeResource ] type = %s, name = \"%s\", no. = %zu owner = \"%s\"",
		res->GetType(), name.data(), owner_ptr->resources.size(), owner_ptr->GetName());

	// Add to owners set of resources
	auto [_, success] = owner_ptr->resources.emplace(res.get());
	VB_ASSERT(success, "Resource already exists!"); // This should never happen
	return res;
};

struct PhysicalDeviceResource : NoCopy {
	InstanceResource* instance = nullptr;
	VkPhysicalDevice handle = VK_NULL_HANDLE;

	// Features
	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT unusedAttachmentFeatures;
	VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphicsPipelineLibraryFeatures;
	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures;
	VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features;
	VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures;
	VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures;
	VkPhysicalDeviceFeatures2 physicalFeatures2;

	// Properties
	VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT graphicsPipelineLibraryProperties;
	VkPhysicalDeviceProperties2 physicalProperties2;
	VkPhysicalDeviceMemoryProperties memoryProperties;

	// Extensions
	std::vector<VkExtensionProperties> availableExtensions;

	// Families
	std::vector<VkQueueFamilyProperties> availableFamilies;

	// Max samples
	SampleCount maxSamples = SampleCount::e1;

	// desiredFlags: required queue flags (strict)
	// undesiredFlags: undesired queue flags (strict)
	// avoidIfPossible: vector of pairs (flags, priority to avoid)
	// surfaceToSupport: surface to support (strict)
	// numTakenQueues: numbers of already taken queues in families (optional)
	// this is rather complicated, but necessary
	struct QueueFamilyIndexRequest {
		struct AvoidInfo{
			VkQueueFlags flags;
			float penalty;
		};
		VkQueueFlags desiredFlags;
		VkQueueFlags undesiredFlags;
		std::span<AvoidInfo const> avoidIfPossible = {};
		VkSurfaceKHR surfaceToSupport = VK_NULL_HANDLE;
		std::span<u32 const> numTakenQueues = {};
	};
	
	u32 static constexpr QUEUE_NOT_FOUND = ~0u;
	u32 static constexpr ALL_SUITABLE_QUEUES_TAKEN = ~1u;

	// @return best fitting queue family index or error code if strict requirements not met
	auto GetQueueFamilyIndex(QueueFamilyIndexRequest const& request) -> u32;
	void GetDetails();
	auto FilterSupported(std::span<char const*> extensions) -> std::vector<std::string_view>;
	auto SupportsExtensions(std::span<char const*> extensions) -> bool;
	PhysicalDeviceResource(InstanceResource* instance, VkPhysicalDevice handle = VK_NULL_HANDLE)
		: instance(instance), handle(handle) {};
};

struct InstanceResource: NoCopyNoMove, std::enable_shared_from_this<InstanceResource> {
	VkInstance handle = VK_NULL_HANDLE;
	VkAllocationCallbacks* allocator = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkDebugReportCallbackEXT debugReport = VK_NULL_HANDLE;
	char const* applicationName = "Vulkan";
	char const* engineName = "Engine";

	u32 apiVersion;

	std::vector<bool>              activeLayers; // Available layers
	std::vector<char const*>       activeLayersNames;
	std::vector<VkLayerProperties> layers;

	std::vector<bool>                  activeExtensions;     // Instance Extensions
	std::vector<char const*>           activeExtensionsNames; // Instance Extensions
	std::vector<VkExtensionProperties> extensions;   // Instance Extensions

	std::vector<char const*> requiredInstanceExtensions {
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	};

	std::vector<PhysicalDeviceResource> physicalDevices;

	std::set<ResourceBase<InstanceResource>*> resources;

	InstanceInfo init_info;

	void Create(InstanceInfo const& info);
	void GetPhysicalDevices();

	auto CreateDebugUtilsMessenger(VkDebugUtilsMessengerCreateInfoEXT const& info) -> VkResult;
	auto CreateDebugReportCallback(VkDebugReportCallbackCreateInfoEXT const& info) -> VkResult;

	void DestroyDebugUtilsMessenger();
	void DestroyDebugReportCallback();

	auto GetName() -> char const* { 
		return "instance";
	}

	auto GetType() -> char const* { 
		return "InstanceResource";
	}

	void Free() {
		activeExtensionsNames.clear();
		activeLayersNames.clear();
		physicalDevices.clear();
		VB_LOG_TRACE("[ Free ] Destroying instance...");
		VB_LOG_TRACE("[ Free ] Cleaning up %zu instance resources...", resources.size());

		std::erase_if(resources, [](ResourceBase<InstanceResource>* const res) {
			res->Free();
			return true;
		});

		if (debugMessenger) {
			DestroyDebugUtilsMessenger();
			VB_LOG_TRACE("[ Free ] Destroyed debug messenger.");
			debugMessenger = nullptr;
		}
		if (debugReport) {
			DestroyDebugReportCallback();
			VB_LOG_TRACE("[ Free ] Destroyed debug report callback.");
			debugReport = nullptr;
		}

		vkDestroyInstance(handle, allocator);
		VB_LOG_TRACE("[ Free ] Destroyed instance.");
	}

	~InstanceResource() {
		Free();
	}
};

struct QueueResource : NoCopy {
	DeviceResource* device = nullptr;
	u32 familyIndex = ~0;
	u32 index = 0;
	QueueInfo init_info;
	VkQueue handle = VK_NULL_HANDLE;
};

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

struct DescriptorResource : ResourceBase<DeviceResource> {
	VkDescriptorPool      pool   = VK_NULL_HANDLE;
	VkDescriptorSetLayout layout = VK_NULL_HANDLE;
	VkDescriptorSet       set    = VK_NULL_HANDLE;

	// Bindless Resource IDs for descriptor indexing
	struct BindingInfoExt: public BindingInfo {
		std::vector<u32> resourceIDs;
	};
	std::unordered_map<DescriptorType, BindingInfoExt> bindings;

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

	auto GetType() -> char const* override { return "DescriptorResource"; }
	void Free() override {
		VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
		vkDestroyDescriptorPool(owner->handle, pool, owner->owner->allocator);
		vkDestroyDescriptorSetLayout(owner->handle, layout, owner->owner->allocator);
	}
};

struct BufferResource : ResourceBase<DeviceResource> {
	u32 rid = NullRID;

	VkBuffer handle;
	VmaAllocation allocation;

	u64 size;
	vk::BufferUsageFlags usage;
	MemoryFlags memory;

	using ResourceBase::ResourceBase;

	auto GetType() -> char const* override {  return "BufferResource"; }
	void Free() override {
		VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
		vmaDestroyBuffer(owner->vmaAllocator, handle, allocation);
		if (rid != NullRID) {
			owner->descriptor.resource->PushID(DescriptorType::eStorageBuffer, rid);
		}
	}
};

struct ImageResource : ResourceBase<DeviceResource> {
	u32 rid = NullRID;

	VkImage handle;
	VkImageView view;
	VmaAllocation allocation;
	bool fromSwapchain = false;
	SampleCount samples = SampleCount::e1;
	std::vector<VkImageView> layersView;
	Extent3D extent{};
	vk::ImageUsageFlags usage;
	vk::Format format;
	ImageLayout layout;
	vk::ImageAspectFlags aspect;
	u32 layers = 1;

	using ResourceBase::ResourceBase;

	auto GetType() -> char const* override {  return "ImageResource"; }
	void Free() override {
		VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
		if (!fromSwapchain) {
			for (VkImageView layerView : layersView) {
				vkDestroyImageView(owner->handle, layerView, owner->owner->allocator);
			}
			layersView.clear();
			vkDestroyImageView(owner->handle, view, owner->owner->allocator);
			vmaDestroyImage(owner->vmaAllocator, handle, allocation);
			if (rid != NullRID) {
				if (usage & ImageUsage::eStorage) {
					owner->descriptor.resource->PushID(DescriptorType::eStorageImage, rid);
				}
				if (usage & ImageUsage::eSampled) {
					owner->descriptor.resource->PushID(DescriptorType::eCombinedImageSampler, rid);
				}
				// for (ImTextureID imguiRID : imguiRIDs) {
					// ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)imguiRID);
				// }
			}
		}
	}
};

struct PipelineResource : ResourceBase<DeviceResource> {
	VkPipeline handle;
	VkPipelineLayout layout;
	PipelinePoint point;

	using ResourceBase::ResourceBase;

	void CreatePipelineLayout(std::span<VkDescriptorSetLayout const> descriptorLayouts);

	auto GetType() -> char const* override {  return "PipelineResource"; }
	void Free() override {
		VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
		vkDestroyPipeline(owner->handle, handle, owner->owner->allocator);
		vkDestroyPipelineLayout(owner->handle, layout, owner->owner->allocator);
	}
};

struct CommandResource : ResourceBase<DeviceResource> {
	VkCommandBuffer handle = VK_NULL_HANDLE;
	// QueueResource*  queue = nullptr;
	VkCommandPool   pool  = VK_NULL_HANDLE;
	VkFence         fence = VK_NULL_HANDLE;

	using ResourceBase::ResourceBase;

	void Create(u32 queueFamilyindex);

	auto GetType() -> char const* override { return "CommandResource"; }

	void Free() override {
		VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
		vkDestroyCommandPool(owner->handle, pool, owner->owner->allocator);
		vkDestroyFence(owner->handle, fence, owner->owner->allocator);
	}
};

struct SwapChainResource: ResourceBase<DeviceResource> {
	VkSwapchainKHR  handle  = VK_NULL_HANDLE;
	VkSurfaceCapabilitiesKHR surface_capabilities;
	std::vector<VkSemaphore> image_available_semaphores;
	std::vector<VkSemaphore> render_finished_semaphores;
	std::vector<VkPresentModeKHR>   available_present_modes;
	std::vector<VkSurfaceFormatKHR> available_surface_formats;

	std::vector<Command> commands;
	std::vector<Image> images;

	u32 current_frame = 0;
	u32 current_image_index = 0;
	bool dirty = true;

	SwapchainInfo init_info = {{}, {}, {}};
	Extent2D extent;

	auto GetImGuiInfo() -> ImGuiInitInfo;

	inline auto GetImageAvailableSemaphore(u32 current_frame) -> VkSemaphore {
		return image_available_semaphores[current_frame];
	}
	inline auto GetRenderFinishedSemaphore(u32 current_frame) -> VkSemaphore {
		return render_finished_semaphores[current_frame];
	}
	auto SupportsFormat(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features) -> bool;
	void CreateSurfaceFormats();
	auto ChooseSurfaceFormat(SurfaceFormat const& desired_surface_format) -> SurfaceFormat;
	auto ChoosePresentMode(PresentMode const& desired_present_mode) -> PresentMode;
	auto ChooseExtent(Extent2D const& desired_extent) -> Extent2D;
	void CreateSwapchain();
	void CreateImages();
	void CreateSemaphores();
	void CreateCommands(u32 queueFamilyindex);
	void Create(SwapchainInfo const& info);
	// void ReCreate(u32 width, u32 height);
		
	auto GetType() -> char const* override {  return "SwapChainResource"; }
	using ResourceBase::ResourceBase;
	void Free() override;
};

auto Buffer::GetResourceID() const -> u32 {
	VB_ASSERT(resource->rid != NullRID, "Invalid buffer rid");
	return resource->rid;
}

auto Buffer::GetSize() const -> u64 {
	return resource->size;
}

auto Buffer::GetMappedData() -> void* {
	VB_ASSERT(resource->memory & Memory::CPU, "Buffer not cpu accessible!");
	return resource->allocation->GetMappedData();
}

auto Buffer::GetAddress() -> DeviceAddress {
	VkBufferDeviceAddressInfo info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = resource->handle,
	};
	return vkGetBufferDeviceAddress(resource->owner->handle, &info);
}

auto Buffer::Map() -> void* {
	VB_ASSERT(resource->memory & Memory::CPU, "Buffer not cpu accessible!");
	void* data;
	vmaMapMemory(resource->owner->vmaAllocator, resource->allocation, &data);
	return data;
}

void Buffer::Unmap() {
	VB_ASSERT(resource->memory & Memory::CPU, "Buffer not cpu accessible!");
	vmaUnmapMemory(resource->owner->vmaAllocator, resource->allocation);
}

auto Image::GetResourceID() const -> u32 {
	VB_ASSERT(resource->rid != NullRID, "Invalid image rid");
	return resource->rid;
}

auto Image::GetFormat() const -> vk::Format {
	return resource->format;
}

void SetLogLevel(LogLevel level) {
	global_log_level = level;
}

auto CreateInstance(InstanceInfo const& info) -> Instance {
	Instance instance;
	VB_LOG_TRACE("[ CreateInstance ]");
	instance.resource = std::make_shared<InstanceResource>();
	instance.resource->Create(info);
	instance.resource->GetPhysicalDevices();
	return instance;
}

auto Instance::CreateDevice(DeviceInfo const& info) -> Device {
	Device device;
	device.resource = MakeResource<DeviceResource>(resource.get(), "device ");
	device.resource->Create(info);
	return device;
}

auto Instance::GetHandle() const -> vk::Instance {
	return resource->handle;
}

void Device::UseDescriptor(Descriptor const& descriptor) {
	resource->descriptor = descriptor;
}

auto Device::GetBinding(DescriptorType const type) -> u32 {
	return resource->descriptor.GetBinding(type);
}

void Device::ResetStaging() {
	resource->stagingOffset = 0;
}

auto Device::GetStagingPtr() -> u8* {
	return resource->stagingCpu + resource->stagingOffset;
}

auto Device::GetStagingOffset() -> u32 {
	return resource->stagingOffset;
}

auto Device::GetStagingSize() -> u32 {
	return resource->init_info.staging_buffer_size;
}

auto Device::GetMaxSamples() -> SampleCount {
	return resource->physicalDevice->maxSamples;
}

void Device::WaitQueue(Queue const& queue) {
	auto result = vkQueueWaitIdle(queue.resource->handle);
	VB_CHECK_VK_RESULT(resource->owner->init_info.checkVkResult, result, "Failed to wait idle command buffer");
}

void Device::WaitIdle() {
	auto result = vkDeviceWaitIdle(resource->handle);
	VB_CHECK_VK_RESULT(resource->owner->init_info.checkVkResult, result, "Failed to wait idle command buffer");
}

auto Queue::GetInfo() const -> QueueInfo {
	return resource->init_info;
}

auto Queue::GetFamilyIndex() const -> u32 {
	return resource->familyIndex;
}

auto Queue::GetHandle() const -> vk::Queue {
	return resource->handle;
}

auto Device::GetQueue(QueueInfo const& info) -> Queue {
	for (auto& q : resource->queuesResources) {
		if ((q.init_info.flags & info.flags) == info.flags &&
				(bool(info.supported_surface) || 
					q.init_info.supported_surface == info.supported_surface)) {
			Queue queue;
			queue.resource = &q;
			return queue;
		}
	}
	return Queue{};
}

auto Device::GetQueues() -> std::span<Queue> {
	return resource->queues;
}

auto Device::CreateBuffer(BufferInfo const& info) -> Buffer {
	return resource->CreateBuffer(info);
}

auto Device::CreateImage(ImageInfo const& info) -> Image {
	return resource->CreateImage(info);
}

auto Descriptor::GetBinding(DescriptorType type) -> u32 {
	return resource->GetBinding(type);
}

auto DeviceResource::CreateBuffer(BufferInfo const& info) -> Buffer {
	vk::BufferUsageFlags usage = info.usage;
	u32 size = info.size;

	if (usage & BufferUsage::eVertexBuffer) {
		usage |= BufferUsage::eTransferDst;
	}

	if (usage & BufferUsage::eIndexBuffer) {
		usage |= BufferUsage::eTransferDst;
	}

	if (usage & BufferUsage::eStorageBuffer) {
		usage |= BufferUsage::eShaderDeviceAddress;
		size += size % physicalDevice->physicalProperties2.properties.limits.minStorageBufferOffsetAlignment;
	}

	if (usage & BufferUsage::eAccelerationStructureBuildInputReadOnlyKHR) {
		usage |= BufferUsage::eShaderDeviceAddress;
		usage |= BufferUsage::eTransferDst;
	}

	if (usage & BufferUsage::eAccelerationStructureStorageKHR) {
		usage |= BufferUsage::eShaderDeviceAddress;
	}

	auto res = MakeResource<BufferResource>(this, info.name);
	Buffer buffer;
	buffer.resource = res;
	res->size   = size;
	res->usage  = usage;
	res->memory = info.memory;
	
	VkBufferCreateInfo bufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VkBufferUsageFlags(usage),
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateFlags constexpr cpuFlags = {
		VMA_ALLOCATION_CREATE_MAPPED_BIT |
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
	};

	VmaAllocationCreateInfo allocInfo = {
		.flags = info.memory & Memory::CPU ? cpuFlags: 0,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	VB_LOG_TRACE("[ vmaCreateBuffer ] name = %s, size = %zu", info.name.data(), bufferInfo.size);
	VB_VK_RESULT result = vmaCreateBuffer(vmaAllocator, &bufferInfo, &allocInfo, &res->handle, &res->allocation, nullptr);
	VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create buffer!");

	// Update bindless descriptor
	if (usage & BufferUsage::eStorageBuffer) {
		VB_ASSERT(descriptor.resource != nullptr, "Descriptor resource not set!");
		res->rid = descriptor.resource->PopID(DescriptorType::eStorageBuffer);

		VkDescriptorBufferInfo bufferInfo = {
			.buffer = res->handle,
			.offset = 0,
			.range  = size,
		};

		VkWriteDescriptorSet write = {
			.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = descriptor.resource->set,
			.dstBinding      = descriptor.resource->GetBinding(DescriptorType::eStorageBuffer),
			.dstArrayElement = buffer.GetResourceID(),
			.descriptorCount = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo     = &bufferInfo,
		};

		vkUpdateDescriptorSets(handle, 1, &write, 0, nullptr);
	}
	return buffer;
}

auto DeviceResource::CreateImage(ImageInfo const& info) -> Image {
	Image image;
	image.resource = MakeResource<ImageResource>(this, info.name);
	auto& res    = image.resource;
	res->samples = info.samples;
	res->extent  = info.extent;
	res->usage   = info.usage;
	res->format  = info.format;
	res->layout  = ImageLayout::eUndefined;
	res->layers  = info.layers;

	VkImageCreateInfo imageInfo{
		.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags         = (VkImageCreateFlags)(info.layers == 6? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT: 0),
		.imageType     = VK_IMAGE_TYPE_2D,
		.format        = VkFormat(info.format),
		.extent = {
			.width     = info.extent.width,
			.height    = info.extent.height,
			.depth     = info.extent.depth,
		},
		.mipLevels     = 1,
		.arrayLayers   = info.layers,
		.samples       = (VkSampleCountFlagBits)std::min(info.samples, physicalDevice->maxSamples),
		.tiling        = VK_IMAGE_TILING_OPTIMAL,
		.usage         = (VkImageUsageFlags)info.usage,
		.sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VmaAllocationCreateInfo allocInfo = {
		.usage = VMA_MEMORY_USAGE_AUTO,
		.preferredFlags = VkMemoryPropertyFlags(info.usage & ImageUsage::eTransientAttachment
			? VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
			: 0),
	};

	VB_LOG_TRACE("[ vmaCreateImage ] name = %s, extent = %ux%ux%u, layers = %u",
		info.name.data(), info.extent.width, info.extent.height, info.extent.depth, info.layers);
	VB_VK_RESULT result = vmaCreateImage(vmaAllocator, &imageInfo, &allocInfo, &res->handle, &res->allocation, nullptr);
	VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create image!");
	
	vk::ImageAspectFlags aspect{};
	switch (info.format) {
		case vk::Format::eD24UnormS8Uint:
			aspect = Aspect::eStencil; // Invalid, cannot be both stencil and depth, todo: create separate imageview
		[[fallthrough]];
		case vk::Format::eD32Sfloat:
		case vk::Format::eD16Unorm:
			aspect |= Aspect::eDepth;
			break;
		[[likely]]
		default:
			aspect = Aspect::eColor;
			break;
	}
	res->aspect = aspect;

	VkImageViewCreateInfo viewInfo{
		.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image    = res->handle,
		.viewType = info.layers == 1? VK_IMAGE_VIEW_TYPE_2D: VK_IMAGE_VIEW_TYPE_CUBE,
		.format   = (VkFormat)info.format,
		.subresourceRange {
			.aspectMask     = VkImageAspectFlags(aspect),
			.baseMipLevel   = 0,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = info.layers
		}
	};

	// TODO(nm): Create image view only if usage if Sampled or Storage or other fitting
	result = vkCreateImageView(handle, &viewInfo, owner->allocator, &res->view);
	VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create image view!");

	if (info.layers > 1) {
		viewInfo.subresourceRange.layerCount = 1;
		res->layersView.resize(info.layers);
		for (u32 i = 0; i < info.layers; i++) {
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.subresourceRange.baseArrayLayer = i;
			result = vkCreateImageView(handle, &viewInfo, owner->allocator, &res->layersView[i]);
			VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create image view!");
		}
	}
	if (info.usage & ImageUsage::eSampled) {
		VB_ASSERT(descriptor.resource != nullptr, "Descriptor resource not set!");
		res->rid = descriptor.resource->PopID(DescriptorType::eCombinedImageSampler);
	}
	if (info.usage & ImageUsage::eStorage) {
		VB_ASSERT(descriptor.resource != nullptr, "Descriptor resource not set!");
		res->rid = descriptor.resource->PopID(DescriptorType::eStorageImage);
	}
	if (info.usage & ImageUsage::eSampled) {
		ImageLayout newLayout = ImageLayout::eShaderReadOnlyOptimal;

		vk::ImageAspectFlags ds = Aspect::eDepth | Aspect::eStencil;
		
		if ((aspect & Aspect::eDepth) == Aspect::eDepth) [[unlikely]] {
			newLayout = ImageLayout::eDepthReadOnlyOptimal;
		}
		
		if ((aspect & ds) == ds) [[unlikely]] {
			newLayout = ImageLayout::eDepthStencilReadOnlyOptimal;
		}

		// res->imguiRIDs.resize(desc.layers);
		// if (desc.layers > 1) {
		//     for (int i = 0; i < desc.layers; i++) {
		//         res->imguiRIDs[i] = (ImTextureID)ImGui_ImplVulkan_AddTexture(, res->layersView[i], (VkImageLayout)newLayout);
		//     }
		// } else {
		//     res->imguiRIDs[0] = (ImTextureID)ImGui_ImplVulkan_AddTexture(, res->view, (VkImageLayout)newLayout);
		// }

		VkDescriptorImageInfo descriptorInfo = {
			.sampler     = GetOrCreateSampler(info.sampler),
			.imageView   = res->view,
			.imageLayout = (VkImageLayout)newLayout,
		};

		VkWriteDescriptorSet write = {
			.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = descriptor.resource->set,
			.dstBinding      = descriptor.resource->GetBinding(DescriptorType::eCombinedImageSampler),
			.dstArrayElement = image.GetResourceID(),
			.descriptorCount = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo      = &descriptorInfo,
		};

		vkUpdateDescriptorSets(handle, 1, &write, 0, nullptr);
	}
	if (info.usage & ImageUsage::eStorage) {
		VkDescriptorImageInfo descriptorInfo = {
			.sampler     = GetOrCreateSampler(info.sampler), // todo: remove
			.imageView   = res->view,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		};
		VkWriteDescriptorSet write = {
			.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = descriptor.resource->set,
			.dstBinding      = descriptor.resource->GetBinding(DescriptorType::eStorageImage),
			.dstArrayElement = image.GetResourceID(),
			.descriptorCount = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.pImageInfo      = &descriptorInfo,
		};

		vkUpdateDescriptorSets(handle, 1, &write, 0, nullptr);
	}

	SetDebugUtilsObjectName({
		.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.objectType   = VkObjectType::VK_OBJECT_TYPE_IMAGE,
		.objectHandle = reinterpret_cast<uint64_t>(res->handle),
		.pObjectName  = info.name.data(),
	});

	SetDebugUtilsObjectName({
		.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.objectType   = VkObjectType::VK_OBJECT_TYPE_IMAGE_VIEW,
		.objectHandle = reinterpret_cast<uint64_t>(res->view),
		.pObjectName  = (std::string(info.name) + "View").data(),
	});
	return image;
}

void Command::Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) {
	vkCmdDispatch(resource->handle, groupCountX, groupCountY, groupCountZ);
}

auto ReadBinaryFile(std::string_view const& path) -> std::vector<char> {
	std::ifstream file(path.data(), std::ios::ate | std::ios::binary);
	// LOG_DEBUG("Reading binary file: " + path);
	VB_ASSERT(file.is_open(), "Failed to open file!");
	size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();
	return buffer;
}

auto CompileShader(Pipeline::Stage const& stage, char const* out_file) -> std::vector<char> {
	char compile_string[VB_MAX_COMPILE_STRING_SIZE];

	if (stage.source.data.ends_with(".slang")){
		std::snprintf(compile_string, VB_MAX_PATH_SIZE, "%s %s -o %s -entry %s %s",
			stage.compiler.data(), stage.source.data.data(), out_file,
				stage.entry_point.data(), stage.compile_options.data());
	} else {
		std::snprintf(compile_string, VB_MAX_PATH_SIZE, "%s -gVS -V %s -o %s -e %s %s", 
			stage.compiler.data(), stage.source.data.data(), out_file,
				stage.entry_point.data(), stage.compile_options.data());
	}

	VB_LOG_TRACE("[ShaderCompiler] Command: %s", compile_string);
	while(std::system(compile_string)) {
		VB_LOG_WARN("[ShaderCompiler] Error! Press something to Compile Again");
		std::getchar();
	}

	return ReadBinaryFile(out_file);
}

auto LoadShader(Pipeline::Stage const& stage) -> std::vector<char> {
	char out_file_name[VB_MAX_PATH_SIZE];
	char out_file[VB_MAX_PATH_SIZE];

	int size = std::snprintf(out_file_name, VB_MAX_PATH_SIZE, "%s", stage.source.data.data());
	std::span file_name_view(out_file_name, size);
	std::replace(file_name_view.begin(), file_name_view.end(), '\\', '-');
	std::replace(file_name_view.begin(), file_name_view.end(), '/', '-');

	if (stage.out_path.empty()) {
		std::snprintf(out_file, VB_MAX_PATH_SIZE, "./%s.spv", file_name_view.data());
	} else {
		std::snprintf(out_file, VB_MAX_PATH_SIZE, "%s/%s.spv", stage.out_path.data(), stage.source.data.data());
	}
	std::filesystem::path const shader_path = stage.source.data;
	std::filesystem::path out_file_path = out_file;
	out_file_path = out_file_path.parent_path();
	bool compilation_required = !stage.allow_skip_compilation;
	
	if (!std::filesystem::exists(out_file_path)){
		std::filesystem::create_directories(out_file_path);
	}
	// check if already compiled to .spv
	if (std::filesystem::exists(out_file) && std::filesystem::exists(shader_path)) {
		auto lastSpvUpdate = std::filesystem::last_write_time(out_file);
		auto lastShaderUpdate = std::filesystem::last_write_time(shader_path);
		if (lastShaderUpdate > lastSpvUpdate) {
			compilation_required = true;
		}
	} else {
		compilation_required = true;
	}
	if (compilation_required) {
		return CompileShader(stage, out_file);
	} else {
		return ReadBinaryFile(out_file);
	}
}

void PipelineResource::CreatePipelineLayout(std::span<VkDescriptorSetLayout const> descriptorLayouts) {
	VkPushConstantRange pushConstant{
		.stageFlags = VK_SHADER_STAGE_ALL,
		.offset     = 0,
		.size       = owner->physicalDevice->physicalProperties2.properties.limits.maxPushConstantsSize,
	};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount         = static_cast<u32>(descriptorLayouts.size()),
		.pSetLayouts            = descriptorLayouts.data(),
		.pushConstantRangeCount = 1,
		.pPushConstantRanges    = &pushConstant,
	};

	VB_VK_RESULT result = vkCreatePipelineLayout(owner->handle, &pipelineLayoutInfo, owner->owner->allocator, &this->layout);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create pipeline layout!");
}

auto Device::CreatePipeline(PipelineInfo const& info) -> Pipeline {
	VB_ASSERT(info.stages.size() > 0, "Pipeline should have at least one stage!");
	if (info.point == PipelinePoint::eGraphics &&
			resource->init_info.pipeline_library &&
				resource->physicalDevice->graphicsPipelineLibraryFeatures.graphicsPipelineLibrary) {
		return resource->pipelineLibrary.CreatePipeline(info);
	}
	return resource->CreatePipeline(info);
}

auto DeviceResource::CreatePipeline(PipelineInfo const& info) -> Pipeline {
	Pipeline pipeline;
	pipeline.resource = MakeResource<PipelineResource>(this, info.name);
	pipeline.resource->point = info.point;
	pipeline.resource->CreatePipelineLayout({{this->descriptor.resource->layout}});

	VB_VLA(VkPipelineShaderStageCreateInfo, shader_stages, info.stages.size());
	VB_VLA(VkShaderModule, shader_modules, info.stages.size());
	for (auto i = 0; auto& stage: info.stages) {
		std::vector<char> bytes = LoadShader(stage);
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = bytes.size();
		createInfo.pCode = (const u32*)(bytes.data());
		VB_VK_RESULT result = vkCreateShaderModule(handle, &createInfo, owner->allocator, &shader_modules[i]);
		VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create shader module!");
		shader_stages[i] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = (VkShaderStageFlagBits)stage.stage,
			.module = shader_modules[i],
			.pName = stage.entry_point.data(),
			.pSpecializationInfo = nullptr,
		};
		++i;
	}

	if (info.point == PipelinePoint::eCompute) {
		VB_ASSERT(shader_stages.size() == 1, "Compute pipeline supports only 1 stage.");
		VkComputePipelineCreateInfo pipelineInfo {
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.stage = shader_stages[0],
			.layout = pipeline.resource->layout,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = -1,
		};
		VB_VK_RESULT result = vkCreateComputePipelines(handle, VK_NULL_HANDLE, 1,
			&pipelineInfo, owner->allocator, &pipeline.resource->handle);
		VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create compute pipeline!");
	} else {
		// graphics pipeline
		VkPipelineRasterizationStateCreateInfo rasterizationState = {
			.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable        = VK_FALSE, // fragments beyond near and far planes are clamped to them
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode             = VK_POLYGON_MODE_FILL,
			.cullMode                = (VkCullModeFlags)info.cullMode,
			.frontFace               = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasEnable         = VK_FALSE,
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp          = 0.0f,
			.depthBiasSlopeFactor    = 0.0f,
			.lineWidth               = 1.0f, // line thickness in terms of number of fragments
		};

		VkPipelineMultisampleStateCreateInfo multisampleState = {
			.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples  = (VkSampleCountFlagBits)std::min(info.samples, physicalDevice->maxSamples),
			.sampleShadingEnable   = VK_FALSE,
			.minSampleShading      = 0.5f,
			.pSampleMask           = nullptr,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable      = VK_FALSE,
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilState = {
			.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable       = VK_TRUE,
			.depthWriteEnable      = VK_TRUE,
			.depthCompareOp        = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable     = VK_FALSE,
			.front                 = {},
			.back                  = {},
			.minDepthBounds        = 0.0f,
			.maxDepthBounds        = 1.0f,
		};

		VB_VLA(VkVertexInputAttributeDescription, attributeDescs, info.vertexAttributes.size());
		u32 attributeSize = 0;
		for (u32 i = 0; auto& format: info.vertexAttributes) {
			// attributeDescs[i++] = VkVertexInputAttributeDescription{ // This gives a bug with gcc (.location starts at 1, not 0)
			attributeDescs[i] = VkVertexInputAttributeDescription{
				.location = i,
				.binding = 0,
				.format = static_cast<VkFormat>(format),
				.offset = attributeSize
			};
			switch (format) {
			case vk::Format::eR32G32Sfloat:       attributeSize += 2 * sizeof(float); break;
			case vk::Format::eR32G32B32Sfloat:    attributeSize += 3 * sizeof(float); break;
			case vk::Format::eR32G32B32A32Sfloat: attributeSize += 4 * sizeof(float); break;
			default: VB_ASSERT(false, "Invalid Vertex Attribute"); break;
			}
			i++; // So we move it here
		}

		VkVertexInputBindingDescription bindingDescription{
			.binding   = 0,
			.stride    = attributeSize,
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		};

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &bindingDescription,
			.vertexAttributeDescriptionCount = static_cast<u32>(attributeDescs.size()),
			.pVertexAttributeDescriptions = attributeDescs.data(),
		};

		// define the type of input of our pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = info.line_topology? VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			// with this parameter true we can break up lines and triangles in _STRIP topology modes
			.primitiveRestartEnable = VK_FALSE,
		};

		VkPipelineDynamicStateCreateInfo dynamicInfo{
			.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = static_cast<u32>(info.dynamicStates.size()),
			.pDynamicStates    = reinterpret_cast<const VkDynamicState*>(info.dynamicStates.data()),
		};

		VkPipelineViewportStateCreateInfo viewportState{
			.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.pViewports    = nullptr,
			.scissorCount  = 1,
			.pScissors     = nullptr,
		};

		VkPipelineRenderingCreateInfoKHR pipelineRendering{
			.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
			.viewMask                = 0,
			.colorAttachmentCount    = static_cast<u32>(info.color_formats.size()),
			.pColorAttachmentFormats = reinterpret_cast<const VkFormat*>(info.color_formats.data()),
			.depthAttachmentFormat   = info.use_depth ? VkFormat(info.depth_format) : VK_FORMAT_UNDEFINED,
			.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
		};

		std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(info.color_formats.size(), {
			.blendEnable         = VK_FALSE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
			.colorBlendOp        = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.colorWriteMask 	 = VK_COLOR_COMPONENT_R_BIT
								 | VK_COLOR_COMPONENT_G_BIT
								 | VK_COLOR_COMPONENT_B_BIT
								 | VK_COLOR_COMPONENT_A_BIT,
		});

		VkPipelineColorBlendStateCreateInfo colorBlendState {
			.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable   = VK_FALSE,
			.logicOp         = VK_LOGIC_OP_COPY,
			.attachmentCount = static_cast<u32>(blendAttachments.size()),
			.pAttachments    = blendAttachments.data(),
			.blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
		};

		VkGraphicsPipelineCreateInfo pipelineInfo{
			.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext               = &pipelineRendering,
			.stageCount          = static_cast<u32>(shader_stages.size()),
			.pStages             = shader_stages.data(),
			.pVertexInputState   = &vertexInputInfo,
			.pInputAssemblyState = &inputAssembly,
			.pViewportState      = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState   = &multisampleState,
			.pDepthStencilState  = &depthStencilState,
			.pColorBlendState    = &colorBlendState,
			.pDynamicState       = &dynamicInfo,
			.layout              = pipeline.resource->layout,
			.basePipelineHandle  = VK_NULL_HANDLE,
			.basePipelineIndex   = -1,
		};

		VB_VK_RESULT vkRes = vkCreateGraphicsPipelines(handle, VK_NULL_HANDLE, 1, 
			&pipelineInfo, owner->allocator, &pipeline.resource->handle);
		VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, vkRes, "Failed to create graphics pipeline!");
	}

	for (auto& shaderModule: shader_modules) {
		vkDestroyShaderModule(handle, shaderModule, owner->allocator);
	}
	return pipeline;
}

auto DeviceResource::PipelineLibrary::CreatePipeline(PipelineInfo const& info) -> Pipeline {
	Pipeline pipeline;
	pipeline.resource = MakeResource<PipelineResource>(device, info.name);
	pipeline.resource->point = info.point;
	pipeline.resource->CreatePipelineLayout({{device->descriptor.resource->layout}});

	DeviceResource::PipelineLibrary::VertexInputInfo vertexInputInfo { info.vertexAttributes, info.line_topology };
	if (!vertexInputInterfaces.count(vertexInputInfo)) {
		CreateVertexInputInterface(vertexInputInfo);
	}

	PipelineLibrary::PreRasterizationInfo preRasterizationInfo {
		.pipelineLayout = pipeline.resource->layout,
		.lineTopology   = info.line_topology,
		.cullMode       = info.cullMode 
	};
	PipelineLibrary::FragmentShaderInfo   fragmentShaderInfo   { pipeline.resource->layout, info.samples};

	auto it_fragment = std::find_if(info.stages.begin(), info.stages.end(), [](Pipeline::Stage const& stage) {
		return stage.stage == ShaderStage::eFragment;
	});
	VB_ASSERT(it_fragment != info.stages.begin(), "Graphics pipeline should have pre-rasterization stage");
	VB_ASSERT(it_fragment != info.stages.end(),   "Graphics pipeline should have fragment stage");

	preRasterizationInfo.stages = {info.stages.begin(), it_fragment};
	fragmentShaderInfo.stages   = {it_fragment, info.stages.end()};

	if (!preRasterizationShaders.contains(preRasterizationInfo)) {
		CreatePreRasterizationShaders(preRasterizationInfo);
	}
	if (!fragmentShaders.contains(fragmentShaderInfo)) {
		CreateFragmentShader(fragmentShaderInfo);
	}

	PipelineLibrary::FragmentOutputInfo fragmentOutputInfo {
		.pipelineLayout = pipeline.resource->layout,
		.colorFormats   = info.color_formats,
		.useDepth       = info.use_depth,
		.depthFormat    = info.depth_format,
		.samples        = info.samples
	};

	if (!fragmentOutputInterfaces.contains(fragmentOutputInfo)) {
		CreateFragmentOutputInterface(fragmentOutputInfo);
	}

	pipeline.resource->handle = LinkPipeline({
			vertexInputInterfaces[vertexInputInfo],
			preRasterizationShaders[preRasterizationInfo],
			fragmentShaders[fragmentShaderInfo],
			fragmentOutputInterfaces[fragmentOutputInfo]
		},
		pipeline.resource->layout
	);

	return pipeline;
}

void DeviceResource::PipelineLibrary::CreateVertexInputInterface(VertexInputInfo const& info) {
	VkGraphicsPipelineLibraryCreateInfoEXT libraryInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
		.flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT,
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = info.lineTopology? VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		// with this parameter true we can break up lines and triangles in _STRIP topology modes
		.primitiveRestartEnable = VK_FALSE,
	};

	VB_VLA(VkVertexInputAttributeDescription, attributeDescs, info.vertexAttributes.size());
	u32 attributeSize = 0;
	for (u32 i = 0; auto& format: info.vertexAttributes) {
		attributeDescs[i] = VkVertexInputAttributeDescription{
			.location = i,
			.binding = 0,
			.format = VkFormat(format),
			.offset = attributeSize
		};
		switch (format) {
		case vk::Format::eR32G32Sfloat:       attributeSize += 2 * sizeof(float); break;
		case vk::Format::eR32G32B32Sfloat:    attributeSize += 3 * sizeof(float); break;
		case vk::Format::eR32G32B32A32Sfloat: attributeSize += 4 * sizeof(float); break;
		default: VB_ASSERT(false, "Invalid Vertex Attribute"); break;
		}
		++i;
	}

	VkVertexInputBindingDescription bindingDescription{
		.binding = 0,
		.stride = attributeSize,
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindingDescription,
		.vertexAttributeDescriptionCount = (u32)(attributeDescs.size()),
		.pVertexAttributeDescriptions = attributeDescs.data(),
	};

	VkGraphicsPipelineCreateInfo pipelineLibraryInfo {
		.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext               = &libraryInfo,
		.flags               = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR
								| VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT,
		.pVertexInputState   = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pDynamicState       = nullptr
	};

	VkPipeline pipeline;
	VB_VK_RESULT res = vkCreateGraphicsPipelines(device->handle, device->pipelineCache, 1, 
		&pipelineLibraryInfo, device->owner->allocator, &pipeline);
	VB_CHECK_VK_RESULT(device->owner->init_info.checkVkResult, res, "Failed to create vertex input interface!");
	vertexInputInterfaces.emplace(info, pipeline);
}

void DeviceResource::PipelineLibrary::CreatePreRasterizationShaders(PreRasterizationInfo const& info) {
	VkGraphicsPipelineLibraryCreateInfoEXT libraryInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
		.flags = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT,
	};

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	if (info.lineTopology) {
		dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
	}

	VkPipelineDynamicStateCreateInfo dynamicInfo{
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<u32>(dynamicStates.size()),
		.pDynamicStates    = dynamicStates.data(),
	};

	VkPipelineViewportStateCreateInfo viewportState{
		.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports    = nullptr,
		.scissorCount  = 1,
		.pScissors     = nullptr,
	};

	VkPipelineRasterizationStateCreateInfo rasterizationState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE, // fragments beyond near and far planes are clamped to them
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = (VkCullModeFlags)info.cullMode, // line thickness in terms of number of fragments
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f,
	};

	// Using the pipeline library extension, we can skip the pipeline shader module creation and directly pass the shader code to the pipeline
	// std::vector<VkShaderModuleCreateInfo> shaderModuleInfos;
	// shaderModuleInfos.reserve(info.stages.size());
	// std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
	// shader_stages.reserve(info.stages.size());
	// std::vector<std::vector<char>> bytes;
	// bytes.reserve(info.stages.size());
	// for (auto& stage: info.stages) {
	// 	bytes.emplace_back(LoadShader(stage));

	// 	shaderModuleInfos.emplace_back(VkShaderModuleCreateInfo{
	// 		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	// 		.codeSize =  bytes.size(),
	// 		.pCode    = (const u32*)bytes.data()
	// 	});

	// 	shader_stages.emplace_back(VkPipelineShaderStageCreateInfo {
	// 		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	// 		.pNext = &shaderModuleInfos.back(),
	// 		.stage = (VkShaderStageFlagBits)stage.stage,
	// 		.pName = stage.entryPoint.data(),
	// 		.pSpecializationInfo = nullptr,
	// 	});
	// }

	std::vector<VkShaderModuleCreateInfo> shaderModuleInfos(info.stages.size());
	std::vector<VkPipelineShaderStageCreateInfo> shader_stages(info.stages.size());
	std::vector<std::vector<char>> bytes(info.stages.size());
	for (size_t i = 0; i < info.stages.size(); ++i) {
		auto& stage = info.stages[i];

		bytes[i] = LoadShader(stage);

		shaderModuleInfos[i] = VkShaderModuleCreateInfo{
			.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = bytes[i].size(),
			.pCode    = (const u32*)bytes[i].data()
		};

		shader_stages[i] = VkPipelineShaderStageCreateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = &shaderModuleInfos[i],
			.stage = (VkShaderStageFlagBits)stage.stage,
			.pName = stage.entry_point.data(),
			.pSpecializationInfo = nullptr,
		};
	}

	VkGraphicsPipelineCreateInfo pipelineLibraryInfo {
		.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext               = &libraryInfo,
		.flags               = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR |
								VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT,
		.stageCount          = static_cast<u32>(shader_stages.size()),
		.pStages             = shader_stages.data(),
		.pViewportState      = &viewportState,
		.pRasterizationState = &rasterizationState,
		.pDynamicState       = &dynamicInfo,
		.layout              = info.pipelineLayout,
	};
	VkPipeline pipeline;
	VB_VK_RESULT res = vkCreateGraphicsPipelines(device->handle, device->pipelineCache, 1,
		&pipelineLibraryInfo, device->owner->allocator, &pipeline);
	VB_CHECK_VK_RESULT(device->owner->init_info.checkVkResult, res,
		"Failed to create pre-rasterization shaders!");
	preRasterizationShaders.emplace(info, pipeline);
}

void DeviceResource::PipelineLibrary::CreateFragmentShader(FragmentShaderInfo const& info) {
	VkGraphicsPipelineLibraryCreateInfoEXT libraryInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
		.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT,
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.front = {},
		.back = {},
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f,
	};

	VkPipelineMultisampleStateCreateInfo multisampleState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = (VkSampleCountFlagBits)std::min(info.samples, device->physicalDevice->maxSamples),
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 0.5f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};


	std::vector<VkShaderModuleCreateInfo> shaderModuleInfos(info.stages.size());
	std::vector<VkPipelineShaderStageCreateInfo> shader_stages(info.stages.size());
	std::vector<std::vector<char>> bytes(info.stages.size());
	for (size_t i = 0; i < info.stages.size(); ++i) {
		auto& stage = info.stages[i];

		bytes[i] = LoadShader(stage);

		shaderModuleInfos[i] = VkShaderModuleCreateInfo{
			.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = bytes[i].size(),
			.pCode    = (const u32*)bytes[i].data()
		};

		shader_stages[i] = VkPipelineShaderStageCreateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = &shaderModuleInfos[i],
			.stage = (VkShaderStageFlagBits)stage.stage,
			.pName = stage.entry_point.data(),
			.pSpecializationInfo = nullptr,
		};
	}

	VkGraphicsPipelineCreateInfo pipelineLibraryInfo {
		.sType              = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext              = &libraryInfo,
		.flags              = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | 
								VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT,
		.stageCount          = static_cast<u32>(shader_stages.size()),
		.pStages             = shader_stages.data(),
		.pMultisampleState  = &multisampleState,
		.pDepthStencilState = &depthStencilState,
		.layout             = info.pipelineLayout,
	};

	//todo: Thread pipeline cache
	VkPipeline pipeline;
	VB_VK_RESULT res = vkCreateGraphicsPipelines(device->handle, device->pipelineCache, 1,
		&pipelineLibraryInfo, nullptr, &pipeline);
	VB_CHECK_VK_RESULT(device->owner->init_info.checkVkResult, res, "Failed to create vertex input interface!");
	fragmentShaders.emplace(info, pipeline);
}

void DeviceResource::PipelineLibrary::CreateFragmentOutputInterface(FragmentOutputInfo const& info) {
	VkGraphicsPipelineLibraryCreateInfoEXT libraryInfo {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
		.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT,
	};

	std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(info.colorFormats.size(), {
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
						| VK_COLOR_COMPONENT_G_BIT
						| VK_COLOR_COMPONENT_B_BIT
						| VK_COLOR_COMPONENT_A_BIT,
	});

	VkPipelineColorBlendStateCreateInfo colorBlendState {
		.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable   = VK_FALSE,
		.logicOp         = VK_LOGIC_OP_COPY,
		.attachmentCount = static_cast<u32>(blendAttachments.size()),
		.pAttachments    = blendAttachments.data(),
		.blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
	};

	VkPipelineMultisampleStateCreateInfo multisampleState {
		.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples  = (VkSampleCountFlagBits)std::min(info.samples, device->physicalDevice->maxSamples),
		.sampleShadingEnable   = VK_FALSE,
		.minSampleShading      = 0.5f,
		.pSampleMask           = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable      = VK_FALSE,
	};

	VkPipelineRenderingCreateInfo pipelineRenderingInfo {
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		.pNext                   = &libraryInfo,
		.viewMask                = 0,
		.colorAttachmentCount    = static_cast<u32>(info.colorFormats.size()),
		.pColorAttachmentFormats = reinterpret_cast<VkFormat const*>(info.colorFormats.data()),
		.depthAttachmentFormat   = info.useDepth ? (VkFormat)info.depthFormat : VK_FORMAT_UNDEFINED,
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	};


	VkGraphicsPipelineCreateInfo pipelineLibraryInfo {
		.sType             = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext             = &pipelineRenderingInfo,
		.flags             = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT,
		.pMultisampleState = &multisampleState,
		.pColorBlendState  = &colorBlendState,
		.layout            = info.pipelineLayout,
	};

	VkPipeline pipeline;
	VB_VK_RESULT result = vkCreateGraphicsPipelines(device->handle, device->pipelineCache, 1,
		&pipelineLibraryInfo, nullptr, &pipeline);
	VB_CHECK_VK_RESULT(device->owner->init_info.checkVkResult, result, "Failed to create fragment output interface!");
	fragmentOutputInterfaces.emplace(info, pipeline);
}

auto DeviceResource::PipelineLibrary::LinkPipeline(std::array<VkPipeline, 4> const& pipelines, VkPipelineLayout layout) -> VkPipeline {

	// Link the library parts into a graphics pipeline
	VkPipelineLibraryCreateInfoKHR linkingInfo {
		.sType        = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,
		.libraryCount = static_cast<u32>(pipelines.size()),
		.pLibraries   = pipelines.data(),
	};

	VkGraphicsPipelineCreateInfo pipelineInfo {
		.sType  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext  = &linkingInfo,
		.layout = layout,
	};

	if (device->init_info.link_time_optimization) {
		pipelineInfo.flags = VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;
	}

	//todo: Thread pipeline cache
	VkPipeline pipeline = VK_NULL_HANDLE;
	VB_VK_RESULT result = vkCreateGraphicsPipelines(device->handle, device->pipelineCache, 1, &pipelineInfo, nullptr, &pipeline);
	VB_CHECK_VK_RESULT(device->owner->init_info.checkVkResult, result, "Failed to create vertex input interface!");
	return pipeline;
}

void DeviceResource::SetDebugUtilsObjectName(VkDebugUtilsObjectNameInfoEXT const& pNameInfo) {
	if (vkSetDebugUtilsObjectNameEXT) {
		vkSetDebugUtilsObjectNameEXT(handle, &pNameInfo);
	}
}

void Command::BeginRendering(RenderingInfo const& info) {
	// auto& clearColor = info.clearColor;
	// auto& clearDepth = info.clearDepth;
	// auto& clearStencil = info.clearStencil;

	Rect2D renderArea = info.renderArea;
	if (renderArea == Rect2D{}) {
		if (info.colorAttachs.size() > 0) {
			renderArea.extent.width = info.colorAttachs[0].colorImage.resource->extent.width;
			renderArea.extent.height = info.colorAttachs[0].colorImage.resource->extent.height;
		} else if (info.depth.image.resource) {
			renderArea.extent.width = info.depth.image.resource->extent.width;
			renderArea.extent.height = info.depth.image.resource->extent.height;
		}
	}

	VkOffset2D offset(renderArea.offset.x, renderArea.offset.y);
	VkExtent2D extent(renderArea.extent.width, renderArea.extent.height);

	VB_VLA(VkRenderingAttachmentInfo, colorAttachInfos, info.colorAttachs.size());
	for (auto i = 0; auto& colorAttach: info.colorAttachs) {
		colorAttachInfos[i] = {
			.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView   = colorAttach.colorImage.resource->view,
			.imageLayout = VkImageLayout(colorAttach.colorImage.resource->layout),
			.loadOp      = VkAttachmentLoadOp(colorAttach.loadOp),
			.storeOp     = VkAttachmentStoreOp(colorAttach.storeOp),
			.clearValue  = *reinterpret_cast<VkClearValue const*>(&colorAttach.clearValue),
		};
		if (colorAttach.resolveImage.resource) {
			colorAttachInfos[i].resolveMode        = VK_RESOLVE_MODE_AVERAGE_BIT;
			colorAttachInfos[i].resolveImageView   = colorAttach.resolveImage.resource->view;
			colorAttachInfos[i].resolveImageLayout = VkImageLayout(colorAttach.resolveImage.resource->layout);
		}
		++i;
	}

	VkRenderingInfoKHR renderingInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
		.flags = 0,
		.renderArea = {
			.offset = offset,
			.extent = extent,
		},
		.layerCount = info.layerCount,
		.viewMask = 0,
		.colorAttachmentCount = static_cast<u32>(colorAttachInfos.size()),
		.pColorAttachments = colorAttachInfos.data(),
		.pDepthAttachment = nullptr,
		.pStencilAttachment = nullptr,
	};

	VkRenderingAttachmentInfo depthAttachInfo;
	if (info.depth.image.resource) {
		depthAttachInfo = {
			.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView   = info.depth.image.resource->view,
			.imageLayout = VkImageLayout(info.depth.image.resource->layout),
			.loadOp      = VkAttachmentLoadOp(info.depth.loadOp),
			.storeOp     = VkAttachmentStoreOp(info.depth.storeOp),
			// .storeOp = info.depth.image.resource->usage & ImageUsage::eTransientAttachment 
			//	? VK_ATTACHMENT_STORE_OP_DONT_CARE
			//	: VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue {
				.depthStencil = { info.depth.clearValue.depth, info.depth.clearValue.stencil }
			}
		};
		renderingInfo.pDepthAttachment = &depthAttachInfo;
	}
	VkRenderingAttachmentInfo stencilAttachInfo;
	if (info.stencil.image.resource) {
		stencilAttachInfo = {
			.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView   = info.stencil.image.resource->view,
			.imageLayout = VkImageLayout(info.stencil.image.resource->layout),
			.loadOp      = VkAttachmentLoadOp(info.stencil.loadOp),
			.storeOp     = VkAttachmentStoreOp(info.stencil.storeOp),
			.clearValue {
				.depthStencil = { info.stencil.clearValue.depth, info.stencil.clearValue.stencil }
			}
		};
		renderingInfo.pStencilAttachment = &stencilAttachInfo;
	}
	vkCmdBeginRendering(resource->handle, &renderingInfo);
}

void Command::SetViewport(Viewport const& viewport) {
	VkViewport vkViewport {
		.x        = viewport.x,
		.y        = viewport.y,
		.width    = viewport.width,
		.height   = viewport.height,
		.minDepth = viewport.minDepth,
		.maxDepth = viewport.maxDepth,
	};
	vkCmdSetViewport(resource->handle, 0, 1, &vkViewport);
}

void Command::SetScissor(Rect2D const& scissor) {
	VkRect2D vkScissor {
		.offset = { scissor.offset.x, scissor.offset.y },
		.extent = {
			.width  = scissor.extent.width,
			.height = scissor.extent.height
		}
	};
	vkCmdSetScissor(resource->handle, 0, 1, &vkScissor);
}

void Command::EndRendering() {
	vkCmdEndRendering(resource->handle);
}

void Command::Draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance) {
	// VB_LOG_TRACE("CmdDraw(%u,%u,%u,%u)", vertex_count, instance_count, first_vertex, first_instance);
	vkCmdDraw(resource->handle, vertex_count, instance_count, first_vertex, first_instance);
}

void Command::DrawIndexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance) {
	// VB_LOG_TRACE("CmdDrawIndexed(%u,%u,%u,%u,%u)", index_count, instance_count, first_index, vertex_offset, first_instance);
	vkCmdDrawIndexed(resource->handle, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void Command::DrawMesh(Buffer const& vertex_buffer, Buffer const& index_buffer, u32 index_count) {
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(resource->handle, 0, 1, &vertex_buffer.resource->handle, offsets);
	vkCmdBindIndexBuffer(resource->handle, index_buffer.resource->handle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(resource->handle, index_count, 1, 0, 0, 0);
}

void Command::BindVertexBuffer(Buffer const& vertex_buffer) {
	VkDeviceSize offsets[] = { 0 };
	VkBuffer buffer;
	if (vertex_buffer.resource) {
		buffer = vertex_buffer.resource->handle;
	} else {
		buffer = VK_NULL_HANDLE;
	}
	vkCmdBindVertexBuffers(resource->handle, 0, 1, &buffer, offsets);
}

void Command::BindIndexBuffer(Buffer const& index_buffer) {
	vkCmdBindIndexBuffer(resource->handle, index_buffer.resource->handle, 0, VK_INDEX_TYPE_UINT32);
}

void Command::BindPipeline(Pipeline const& pipeline) {
	vkCmdBindPipeline(resource->handle, (VkPipelineBindPoint)pipeline.resource->point, pipeline.resource->handle);
	// TODO(nm): bind only if not compatible for used descriptor sets or push constant range
	// ref: https://registry.khronos.org/vulkan/specs/1.2-extensions/html/vkspec.html#descriptorsets-compatibility
	vkCmdBindDescriptorSets(resource->handle, (VkPipelineBindPoint)pipeline.resource->point,
		pipeline.resource->layout, 0, 1, &resource->owner->descriptor.resource->set, 0, nullptr);
}

void Command::PushConstants(Pipeline const& pipeline, void const* data, u32 size) {
	vkCmdPushConstants(resource->handle, pipeline.resource->layout, VK_SHADER_STAGE_ALL, 0, size, data);
}

// vkWaitForFences + vkResetFences +
// vkResetCommandPool + vkBeginCommandBuffer
void Command::Begin() {
	vkWaitForFences(resource->owner->handle, 1, &resource->fence, VK_TRUE, UINT64_MAX);
	vkResetFences(resource->owner->handle, 1, &resource->fence);

	// ?VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT
	vkResetCommandPool(resource->owner->handle, resource->pool, 0);
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(resource->handle, &beginInfo);
}

void Command::End() {
	vkEndCommandBuffer(resource->handle);
	// resource->currentPipeline = {};
}

void Queue::Submit(
		std::span<CommandBufferSubmitInfo const> cmds,
		Fence fence,
		SubmitInfo const& info) const {

	VkSubmitInfo2 submitInfo {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount = static_cast<u32>(info.waitSemaphoreInfos.size()),
		.pWaitSemaphoreInfos = reinterpret_cast<VkSemaphoreSubmitInfo const*>(info.waitSemaphoreInfos.data()),
		.commandBufferInfoCount = static_cast<u32>(cmds.size()),
		.pCommandBufferInfos = reinterpret_cast<VkCommandBufferSubmitInfo const*>(cmds.data()),
		.signalSemaphoreInfoCount = static_cast<u32>(info.signalSemaphoreInfos.size()),
		.pSignalSemaphoreInfos = reinterpret_cast<VkSemaphoreSubmitInfo const*>(info.signalSemaphoreInfos.data()),
	};

	auto result = vkQueueSubmit2(resource->handle, 1, &submitInfo, fence);
	VB_CHECK_VK_RESULT(resource->device->owner->init_info.checkVkResult, result, "Failed to submit command buffer");
}

void Command::QueueSubmit(Queue const& queue, SubmitInfo const& info) {

	// VkCommandBufferSubmitInfo cmdInfo {
	// 	.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
	// 	.commandBuffer = GetHandle(),
	// };

	// VkSubmitInfo2 submitInfo {
	// 	.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
	// 	.waitSemaphoreInfoCount = static_cast<u32>(info.waitSemaphoreInfos.size()),
	// 	.pWaitSemaphoreInfos = reinterpret_cast<VkSemaphoreSubmitInfo const*>(info.waitSemaphoreInfos.data()),
	// 	.commandBufferInfoCount = 1,
	// 	.pCommandBufferInfos = &cmdInfo,
	// 	.signalSemaphoreInfoCount = static_cast<u32>(info.signalSemaphoreInfos.size()),
	// 	.pSignalSemaphoreInfos = reinterpret_cast<VkSemaphoreSubmitInfo const*>(info.signalSemaphoreInfos.data()),
	// };

	// auto result = vkQueueSubmit2(info.queue.resource->handle, 1, &submitInfo, resource->fence);
	// VB_CHECK_VK_RESULT(resource->owner->owner->init_info.checkVkResult, result, "Failed to submit command buffer");

	// Commented code can be faster but DRY
	queue.Submit(
		{{{.commandBuffer = GetHandle()}}},
		resource->fence, {
			.waitSemaphoreInfos = info.waitSemaphoreInfos,
			.signalSemaphoreInfos = info.signalSemaphoreInfos,
		});
}

auto Command::GetHandle() const -> vk::CommandBuffer {
	return resource->handle;
}

auto Command::GetFence() const -> Fence {
	return resource->fence;
}


auto InstanceResource::CreateDebugUtilsMessenger(
		VkDebugUtilsMessengerCreateInfoEXT const& info ) -> VkResult {
	// search for the requested function and return null if cannot find
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr (
		handle,
		"vkCreateDebugUtilsMessengerEXT"
	);
	if (func != nullptr) {
		return func(handle, &info, allocator, &debugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void InstanceResource::DestroyDebugUtilsMessenger() {
	// search for the requested function and return null if cannot find
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr (
		handle,
		"vkDestroyDebugUtilsMessengerEXT"
	);
	if (func != nullptr) {
		func(handle, debugMessenger, allocator);
	}
}

auto InstanceResource::CreateDebugReportCallback(
	VkDebugReportCallbackCreateInfoEXT const& info ) -> VkResult {
	// search for the requested function and return null if cannot find
	auto func = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr (
		handle,
		"vkCreateDebugReportCallbackEXT"
	);
	if (func != nullptr) {
		return func(handle, &info, allocator, &debugReport);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void InstanceResource::DestroyDebugReportCallback() {
	// search for the requested function and return null if cannot find
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr (
		handle,
		"vkDestroyDebugReportCallbackEXT"
	);
	if (func != nullptr) {
		func(handle, debugReport, allocator);
	}
}

namespace {
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsCallback (
	VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT             messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void*                                       pUserData) {
	// log the message
	// here we can set a minimum severity to log the message
	// if (messageSeverity > VK_DEBUG_UTILS...)
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) 
		{ VB_LOG_TRACE("[ Validation Layer ] %s", pCallbackData->pMessage); }
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)    
		{ VB_LOG_TRACE ("[ Validation Layer ] %s", pCallbackData->pMessage); }
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) 
		{ VB_LOG_WARN ("[ Validation Layer ] %s", pCallbackData->pMessage); }
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)   
		{ VB_LOG_ERROR("[ Validation Layer ] %s", pCallbackData->pMessage); }

	return VK_FALSE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objectType,
	uint64_t object,
	size_t location,
	i32 messageCode,
	char const* pLayerPrefix,
	char const* pMessage,
	void* pUserData)
{
	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)        
		{ VB_LOG_TRACE ("[Debug Report] %s", pMessage); }
	if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)            
		{ VB_LOG_WARN ("[Debug Report] %s", pMessage); }
	if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		{ VB_LOG_WARN ("[Debug Report Performance] %s", pMessage); }
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)              
		{ VB_LOG_ERROR("[Debug Report] %s", pMessage); }
	if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)              
		{ VB_LOG_TRACE("[Debug Report] %s", pMessage); }

	return VK_FALSE;
}

void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	// createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
	createInfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	createInfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;
	createInfo.pfnUserCallback = DebugUtilsCallback;
	createInfo.pUserData = nullptr;
}

void PopulateDebugReportCreateInfo(VkDebugReportCallbackCreateInfoEXT& createInfo) {
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	createInfo.flags = {
		VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
		VK_DEBUG_REPORT_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_ERROR_BIT_EXT |
		VK_DEBUG_REPORT_DEBUG_BIT_EXT
	};
	createInfo.pfnCallback = DebugReportCallback;
}
} // debug report

void InstanceResource::Create(InstanceInfo const& info){
	auto instance = this;
	init_info = info;

	// optional data, provides useful info to the driver
	VkApplicationInfo appInfo{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = applicationName,
		.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
		.pEngineName = engineName,
		.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
		.apiVersion = VK_API_VERSION_1_3,
	};

	// get api version
	vkEnumerateInstanceVersion(&apiVersion);

	// Validation layers
	if (info.validation_layers) {
		// get all available layers
		u32 layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		layers.resize(layerCount);
		activeLayers.resize(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

		// active default khronos validation layer
		std::string_view constexpr validation_layer_name("VK_LAYER_KHRONOS_validation");
		bool khronosAvailable = false;
		for (size_t i = 0; i < layerCount; i++) {
			activeLayers[i] = false;
			if (validation_layer_name == layers[i].layerName) {
				activeLayers[i] = true;
				khronosAvailable = true;
				break;
			}
		}
		if (!khronosAvailable) {VB_LOG_WARN("Default validation layer not available!");}

		for (size_t i = 0; i < layerCount; i++) {
			if (activeLayers[i]) {
				activeLayersNames.push_back(layers[i].layerName);
			}
		}
	}

	requiredInstanceExtensions.insert(requiredInstanceExtensions.end(),
		init_info.extensions.begin(), init_info.extensions.end());
	
	// Extensions
	if (info.validation_layers) {
		requiredInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		if (info.debug_report) {
			requiredInstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
	}

	// get all available extensions
	u32 extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, 0);
	extensions.resize(extensionCount);
	activeExtensions.resize(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	// set to active all extensions that we enabled //TODO(nm): Rewrite loop
	for (size_t i = 0; i < requiredInstanceExtensions.size(); i++) {
		for (size_t j = 0; j < extensionCount; j++) {
			if (std::strcmp(requiredInstanceExtensions[i], extensions[j].extensionName) == 0) {
				activeExtensions[j] = true;
				break;
			}
		}
	}

	// Enable validation features if available
	bool validation_features = false;
	if (info.validation_layers){
		for (size_t i = 0; i < extensionCount; i++) {
			if (std::strcmp(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME, extensions[i].extensionName) == 0) {
				validation_features = true;
				activeExtensions[i] = true;
				VB_LOG_TRACE("%s is available, enabling it", VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
				break;
			}
		}
	}

	VkInstanceCreateInfo createInfo{
		.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo        = &appInfo,
		.enabledExtensionCount   = static_cast<u32>(activeExtensionsNames.size()),
		.ppEnabledExtensionNames = activeExtensionsNames.data(),
	};

	VkValidationFeaturesEXT validationFeaturesInfo;
	VkValidationFeatureEnableEXT enableFeatures[] = {
		VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
		VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
		VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
	};

	if (validation_features) {
		validationFeaturesInfo = {
			.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
			.pNext                         = createInfo.pNext,
			.enabledValidationFeatureCount = std::size(enableFeatures),
			.pEnabledValidationFeatures    = enableFeatures,
		};
		createInfo.pNext = &validationFeaturesInfo;
	}

	// get the name of all extensions that we enabled
	activeExtensionsNames.clear();
	for (size_t i = 0; i < extensionCount; i++) {
		if (activeExtensions[i]) {
			activeExtensionsNames.push_back(extensions[i].extensionName);
		}
	}

	createInfo.enabledExtensionCount = static_cast<u32>(activeExtensionsNames.size());
	createInfo.ppEnabledExtensionNames = activeExtensionsNames.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if (info.validation_layers) {
		createInfo.enabledLayerCount = activeLayersNames.size();
		createInfo.ppEnabledLayerNames = activeLayersNames.data();

		// we need to set up a separate logger just for the instance creation/destruction
		// because our "default" logger is created after
		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		debugCreateInfo.pNext = createInfo.pNext;
		createInfo.pNext      = &debugCreateInfo;
	}

	// Instance creation
	VB_VK_RESULT result = vkCreateInstance(&createInfo, allocator, &handle);
	VB_CHECK_VK_RESULT(init_info.checkVkResult, result, "Failed to create Vulkan instance!");
	VB_LOG_TRACE("Created instance.");

	// Debug Utils
	if (info.validation_layers) {
		VkDebugUtilsMessengerCreateInfoEXT messengerInfo;
		PopulateDebugMessengerCreateInfo(messengerInfo);
		result = CreateDebugUtilsMessenger(messengerInfo);
		VB_CHECK_VK_RESULT(init_info.checkVkResult, result, "Failed to set up debug messenger!");
		VB_LOG_TRACE("Created debug messenger.");
	}

	// Debug Report
	if (info.validation_layers && info.debug_report) {
		VkDebugReportCallbackCreateInfoEXT debugReportInfo;
		PopulateDebugReportCreateInfo(debugReportInfo);
		// Create the callback handle
		result = CreateDebugReportCallback(debugReportInfo);
		VB_CHECK_VK_RESULT(init_info.checkVkResult, result, "Failed to set up debug report callback!");
		VB_LOG_TRACE("Created debug report callback.");
	}

	VB_LOG_TRACE("Created Vulkan Instance.");
}

void InstanceResource::GetPhysicalDevices() {
	// get all devices with Vulkan support
	u32 count = 0;
	vkEnumeratePhysicalDevices(handle, &count, nullptr);
	VB_ASSERT(count != 0, "no GPUs with Vulkan support!");
	VB_VLA(VkPhysicalDevice, physical_devices, count);
	vkEnumeratePhysicalDevices(handle, &count, physical_devices.data());
	VB_LOG_TRACE("Found %d physical device(s).", count);
	physicalDevices.reserve(count);
	for (auto const& device: physical_devices) {
		// device->physicalDevices->handle.insert({UIDGenerator::Next(), PhysicalDevice()});
		physicalDevices.emplace_back(this, device);
		auto& physicalDevice = physicalDevices.back();
		physicalDevice.GetDetails();
	};
}

void PhysicalDeviceResource::GetDetails() {
	// get all available extensions
	u32 extensionCount;
	vkEnumerateDeviceExtensionProperties(handle, nullptr, &extensionCount, nullptr);
	availableExtensions.resize(extensionCount);
	vkEnumerateDeviceExtensionProperties(handle, nullptr, &extensionCount, availableExtensions.data());

	// get all available families
	u32 familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(handle, &familyCount, nullptr);
	availableFamilies.resize(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(handle, &familyCount, availableFamilies.data());

	// get features
	unusedAttachmentFeatures        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT, nullptr };
	graphicsPipelineLibraryFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT, &unusedAttachmentFeatures };
	indexingFeatures                = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,       &graphicsPipelineLibraryFeatures };
	bufferDeviceAddressFeatures     = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,         &indexingFeatures };
	synchronization2Features        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,         &bufferDeviceAddressFeatures };
	dynamicRenderingFeatures        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,             &synchronization2Features };
	physicalFeatures2               = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,                             &dynamicRenderingFeatures };
	vkGetPhysicalDeviceFeatures2(handle, &physicalFeatures2);

	// get properties
	graphicsPipelineLibraryProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT;
	graphicsPipelineLibraryProperties.pNext = nullptr;
	physicalProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	physicalProperties2.pNext = &graphicsPipelineLibraryProperties;
	vkGetPhysicalDeviceProperties2(handle, &physicalProperties2);
	vkGetPhysicalDeviceMemoryProperties(handle, &memoryProperties);

	VkSampleCountFlags counts = physicalProperties2.properties.limits.framebufferColorSampleCounts;

	// Get max number of samples
	for (VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_64_BIT;
			sampleCount >= VK_SAMPLE_COUNT_1_BIT;
				sampleCount = static_cast<VkSampleCountFlagBits>(sampleCount >> 1)) {
		if (counts & sampleCount) {
			maxSamples = static_cast<SampleCount>(sampleCount);
			break;
		}
	}
}

auto PhysicalDeviceResource::FilterSupported(std::span<char const*> extensions) -> std::vector<std::string_view> {
	std::vector<std::string_view> extensionsToEnable;
	extensionsToEnable.reserve(extensions.size());
	std::copy_if(extensions.begin(), extensions.end(), std::back_inserter(extensionsToEnable),
		[&](char const* extension) {
			if (std::any_of(availableExtensions.begin(), availableExtensions.end(),
				[&extension](auto const& availableExtension) {
					return std::string_view(availableExtension.extensionName) == extension;
			}) == true) {
				return true; 
			} else {
				VB_LOG_WARN("Extension %s not supported on device %s",
					extension, physicalProperties2.properties.deviceName);
				return false;
			}
		});
	return extensionsToEnable;
}

auto PhysicalDeviceResource::SupportsExtensions(std::span<char const*> extensions) -> bool {
	return std::all_of(extensions.begin(), extensions.end(), [this](auto const& extension) {
		return std::any_of(availableExtensions.begin(), availableExtensions.end(),
			[&extension](auto const& availableExtension) {
				return std::string_view(availableExtension.extensionName) == extension;
			});
	});
}

auto PhysicalDeviceResource::GetQueueFamilyIndex(const QueueFamilyIndexRequest& params) -> u32 {
	struct Candidate {
		u32   familyIndex;
		float penalty;
	};
	
	std::vector<Candidate> candidates;
	auto& families = availableFamilies;
	
	// Check if queue family is suitable
	for (size_t i = 0; i < families.size(); i++) {
		bool suitable = ((families[i].queueFlags & params.desiredFlags) == params.desiredFlags &&
						((families[i].queueFlags & params.undesiredFlags) == 0));

		// Get surface support
		if (params.surfaceToSupport != VK_NULL_HANDLE) {
			VkBool32 presentSupport = false;
			VB_VK_RESULT result = vkGetPhysicalDeviceSurfaceSupportKHR(handle, i, params.surfaceToSupport, &presentSupport);
			VB_CHECK_VK_RESULT(instance->init_info.checkVkResult, result, "vkGetPhysicalDeviceSurfaceSupportKHR failed!");
			suitable = suitable && presentSupport;
		}

		if (!suitable) {
			continue;
		}

		if (params.avoidIfPossible.empty()) {
			return i;
		}

		Candidate candidate(i, 0.0f);
		
		// Prefer family with least number of VkQueueFlags
		for (VkQueueFlags bit = 1; bit != 0; bit <<= 1) {
			if (families[i].queueFlags & bit) {
				candidate.penalty += 0.01f;
			}
		}

		// Add penalty for supporting unwanted VkQueueFlags
		for (auto& avoid: params.avoidIfPossible) {
			if ((families[i].queueFlags & avoid.flags) == avoid.flags) {
				candidate.penalty += avoid.penalty;
			}
		}
		
		// Add candidate if family is not filled up
		if (params.numTakenQueues[i] < families[i].queueCount) {
			candidates.push_back(candidate);
		} else {
			VB_LOG_WARN("Queue family %zu is filled up", i);
		}
	}

	if (candidates.empty()) {
		return ALL_SUITABLE_QUEUES_TAKEN;
	}

	u32 bestFamily = std::min_element(candidates.begin(), candidates.end(),
							[](Candidate& l, Candidate& r) {
									return l.penalty < r.penalty; 
								})->familyIndex;
	VB_LOG_TRACE("Best family: %u", bestFamily);
	return bestFamily;
}

void DeviceResource::Create(DeviceInfo const& info) {
	init_info = info;
	pipelineLibrary.device = this;
	// <family index, queue count>
	// VB_VLA(u32, numQueuesToCreate, physicalDevice->availableFamilies.size());
	std ::vector<u32> numQueuesToCreate;
	// std::fill(numQueuesToCreate.begin(), numQueuesToCreate.end(), 0);

	// Add swapchain extension
		if (std::any_of( info.queues.begin(), info.queues.end(),
				[](QueueInfo const &q) { return bool(q.supported_surface); })) {
			requiredExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		}

		for (auto& physicalDevice : owner->physicalDevices) {
			// Require extension support
			if (!physicalDevice.SupportsExtensions(requiredExtensions)) {
				continue;
			}
			numQueuesToCreate.resize(physicalDevice.availableFamilies.size(), 0);
			// std::fill(numQueuesToCreate.begin(), numQueuesToCreate.end(), 0);

			bool deviceSuitable = true;
			for (auto& queue_info: info.queues) {
				// Queue choosing heuristics
				std::span<PhysicalDeviceResource::QueueFamilyIndexRequest::AvoidInfo const> avoid_if_possible{};
				if(queue_info.separate_family) {
					switch (QueueFlags::MaskType(queue_info.flags)) {
					case VK_QUEUE_COMPUTE_BIT: 
						avoid_if_possible = {{{VK_QUEUE_GRAPHICS_BIT, 1.0f}, {VK_QUEUE_TRANSFER_BIT, 0.5f}}};
						break;
					case VK_QUEUE_TRANSFER_BIT:
						avoid_if_possible = {{{VK_QUEUE_GRAPHICS_BIT, 1.0f}, {VK_QUEUE_COMPUTE_BIT, 0.5f}}};
						break;
					default:
						avoid_if_possible = {{{VK_QUEUE_GRAPHICS_BIT, 1.0f}}};
						break;
					}
				}

				// Get family index fitting requirements
				PhysicalDeviceResource::QueueFamilyIndexRequest request{
					.desiredFlags = VkQueueFlags(queue_info.flags),
					.undesiredFlags = 0,
					.avoidIfPossible = avoid_if_possible,			
					.numTakenQueues = numQueuesToCreate
				};

				if (queue_info.supported_surface) {
					request.surfaceToSupport = queue_info.supported_surface;
				}

				auto familyIndex = physicalDevice.GetQueueFamilyIndex(request);
				if (familyIndex == PhysicalDeviceResource::QUEUE_NOT_FOUND) {
					VB_LOG_WARN("Requested queue flags %d not available on device: %s",
						QueueFlags::MaskType(queue_info.flags), physicalDevice.physicalProperties2.properties.deviceName);
					deviceSuitable = false;
					break;
				} else if (familyIndex == PhysicalDeviceResource::ALL_SUITABLE_QUEUES_TAKEN) {
					VB_LOG_WARN("Requested more queues with flags %d than available on device: %s. Queue was not created",
						QueueFlags::MaskType(queue_info.flags), physicalDevice.physicalProperties2.properties.deviceName);
					continue;
				}
				// Create queue
				numQueuesToCreate[familyIndex]++;
			}
		if (deviceSuitable) {
			this->physicalDevice = &physicalDevice;
			break;
		}
	}

	// if (this->physicalDevice == nullptr) {
	// 	VB_LOG_ERROR("Failed to find suitable device");
	// 	// return;
	// }
	VB_ASSERT(this->physicalDevice != nullptr, "Failed to find suitable device");

	name += physicalDevice->physicalProperties2.properties.deviceName;

	u32 maxQueuesInFamily = 0;
	for (auto& family: physicalDevice->availableFamilies) {
		if (family.queueCount > maxQueuesInFamily) {
			maxQueuesInFamily = family.queueCount;
		}
	}

	// priority for each type of queue (1.0f for all)
	VB_VLA(float, priorities, maxQueuesInFamily);
	std::fill(priorities.begin(), priorities.end(), 1.0f);

	int numFamilies = std::count_if(numQueuesToCreate.begin(), numQueuesToCreate.end(), [](int queueCount) {
		return queueCount > 0;
    });

	VB_VLA(VkDeviceQueueCreateInfo, queueCreateInfos, numFamilies);
	for (u32 queueInfoIndex{}, familyIndex{}; familyIndex < numQueuesToCreate.size(); ++familyIndex) {
		if (numQueuesToCreate[familyIndex] == 0) continue;
		queueCreateInfos[queueInfoIndex] = {
			.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = familyIndex,
			.queueCount       = numQueuesToCreate[familyIndex],
			.pQueuePriorities = priorities.data(),
		};
		++queueInfoIndex;
	}

	auto& supportedFeatures = physicalDevice->physicalFeatures2.features;
	VkPhysicalDeviceFeatures2 features2 {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.features = {
			.geometryShader    = supportedFeatures.geometryShader,
			.sampleRateShading = supportedFeatures.sampleRateShading,
			.logicOp           = supportedFeatures.logicOp,
			.depthClamp        = supportedFeatures.depthClamp,
			.fillModeNonSolid  = supportedFeatures.fillModeNonSolid,
			.wideLines         = supportedFeatures.wideLines,
			.multiViewport     = supportedFeatures.multiViewport,
			.samplerAnisotropy = supportedFeatures.samplerAnisotropy,
		}
	};

	// request features if available
	VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphicsPipelineLibraryFeatures;
	if (init_info.pipeline_library && physicalDevice->graphicsPipelineLibraryFeatures.graphicsPipelineLibrary) {
		graphicsPipelineLibraryFeatures = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT,
			.pNext = features2.pNext,
			.graphicsPipelineLibrary = true,
		}; features2.pNext = &graphicsPipelineLibraryFeatures;
		requiredExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
		requiredExtensions.push_back(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
	}

	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
		.pNext = features2.pNext,
		.shaderUniformBufferArrayNonUniformIndexing   = physicalDevice->indexingFeatures.shaderUniformBufferArrayNonUniformIndexing,
		.shaderSampledImageArrayNonUniformIndexing    = physicalDevice->indexingFeatures.shaderSampledImageArrayNonUniformIndexing,
		.shaderStorageBufferArrayNonUniformIndexing   = physicalDevice->indexingFeatures.shaderStorageBufferArrayNonUniformIndexing,
		.descriptorBindingSampledImageUpdateAfterBind = physicalDevice->indexingFeatures.descriptorBindingSampledImageUpdateAfterBind,
		.descriptorBindingStorageImageUpdateAfterBind = physicalDevice->indexingFeatures.descriptorBindingStorageImageUpdateAfterBind,
		.descriptorBindingPartiallyBound              = physicalDevice->indexingFeatures.descriptorBindingPartiallyBound,
		.runtimeDescriptorArray                       = physicalDevice->indexingFeatures.runtimeDescriptorArray,
	}; features2.pNext = &descriptorIndexingFeatures;

	VkPhysicalDeviceBufferDeviceAddressFeatures addresFeatures{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
		.pNext = features2.pNext,
		.bufferDeviceAddress = physicalDevice->bufferDeviceAddressFeatures.bufferDeviceAddress,
	}; features2.pNext = &addresFeatures;

	// VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
	// rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	// rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
	// rayTracingPipelineFeatures.pNext = &addresFeatures;

	// VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
	// accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	// accelerationStructureFeatures.accelerationStructure = VK_TRUE;
	// accelerationStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;
	// accelerationStructureFeatures.accelerationStructureCaptureReplay = VK_TRUE;
	// accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;

	// VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
	// rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
	// rayQueryFeatures.rayQuery = VK_TRUE;
	// // rayQueryFeatures.pNext = &accelerationStructureFeatures;
	// rayQueryFeatures.pNext = &addresFeatures;



	VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
		.pNext = features2.pNext,
		.dynamicRendering = physicalDevice->dynamicRenderingFeatures.dynamicRendering,
	}; features2.pNext = &dynamicRenderingFeatures;

	VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT unusedAttachmentFeatures;
	if (init_info.unused_attachments && physicalDevice->unusedAttachmentFeatures.dynamicRenderingUnusedAttachments) {
		unusedAttachmentFeatures = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT,
			.pNext = features2.pNext,
			.dynamicRenderingUnusedAttachments = true,
		}; features2.pNext = &unusedAttachmentFeatures;
		requiredExtensions.push_back(VK_EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_EXTENSION_NAME);
	}

	VkPhysicalDeviceSynchronization2FeaturesKHR sync2Features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
		.pNext = features2.pNext,
		.synchronization2 = physicalDevice->synchronization2Features.synchronization2,
	}; features2.pNext = &sync2Features;

	// VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicFeatures{};
	// atomicFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
	// atomicFeatures.shaderBufferFloat32AtomicAdd = VK_TRUE;
	// atomicFeatures.pNext = &sync2Features;

	VkDeviceCreateInfo createInfo{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &features2,
		.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size()),
		.pQueueCreateInfos = queueCreateInfos.data(),
		.enabledExtensionCount = static_cast<u32>(requiredExtensions.size()),
		.ppEnabledExtensionNames = requiredExtensions.data(),
	};

	// specify the required layers to the device
	if (owner->init_info.validation_layers) {
		auto& layers = owner->activeLayersNames;
		createInfo.enabledLayerCount = layers.size();
		createInfo.ppEnabledLayerNames = layers.data();
	}

	VB_VK_RESULT result = vkCreateDevice(physicalDevice->handle, &createInfo, owner->allocator, &handle);
	VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create logical device!");
	VB_LOG_TRACE("Created logical device, name = %s", physicalDevice->physicalProperties2.properties.deviceName);

	queuesResources.reserve(std::reduce(numQueuesToCreate.begin(), numQueuesToCreate.end()));
	for (auto& info: queueCreateInfos) {
		for (u32 index = 0; index < info.queueCount; ++index) {
			queuesResources.emplace_back(QueueResource{
				.device = this,
				.familyIndex = info.queueFamilyIndex,
				.index = index,
				.init_info = {
					.flags = static_cast<QueueFlags>(physicalDevice->availableFamilies[info.queueFamilyIndex].queueFlags),
				},
			});
			auto& queue = queuesResources.back();
			vkGetDeviceQueue(handle, info.queueFamilyIndex, index, &queue.handle);
		}
	}

	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocatorCreateInfo = {
		.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT
				| VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
				//| VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT,
		.physicalDevice = physicalDevice->handle,
		.device = handle,
		.pVulkanFunctions = &vulkanFunctions,
		.instance = owner->handle,
		.vulkanApiVersion = VK_API_VERSION_1_3,
	};

	result = vmaCreateAllocator(&allocatorCreateInfo, &vmaAllocator);
	VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create allocator!");

	if (owner->init_info.validation_layers) {
		vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>
			(vkGetDeviceProcAddr(handle, "vkSetDebugUtilsObjectNameEXT"));
	}
	// vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>
	// 	(vkGetDeviceProcAddr(handle, "vkGetAccelerationStructureBuildSizesKHR"));
	// vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>
	// 	(vkGetDeviceProcAddr(handle, "vkCreateAccelerationStructureKHR"));
	// vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>
	// 	(vkGetDeviceProcAddr(handle, "vkCmdBuildAccelerationStructuresKHR"));
	// vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>
	// 	(vkGetDeviceProcAddr(handle, "vkGetAccelerationStructureDeviceAddressKHR"));
	// vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>
	// 	(vkGetDeviceProcAddr(handle, "vkDestroyAccelerationStructureKHR"));

	VkDescriptorPoolSize imguiPoolSizes[] = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000} 
	};

	VkDescriptorPoolCreateInfo imguiPoolInfo = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets       = 1024u,
		.poolSizeCount = std::size(imguiPoolSizes),
		.pPoolSizes    = imguiPoolSizes,
	};

	result = vkCreateDescriptorPool(handle, &imguiPoolInfo, owner->allocator, &imguiDescriptorPool);
	VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create imgui descriptor pool!");
	
	// Create command buffers
	// for (auto& q: queuesResources) {
	// 	q.command.resource = MakeResource<CommandResource>(this, "Command Buffer");
	// 	q.command.resource->Create(&q);
	// }

	// Create staging buffer
	staging = CreateBuffer({
		init_info.staging_buffer_size,
		BufferUsage::eTransferSrc,
		Memory::CPU,
		"Staging Buffer"
	});
	stagingCpu = reinterpret_cast<u8*>(staging.resource->allocation->GetMappedData());
}

bool SwapChainResource::SupportsFormat(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features) {
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties(owner->physicalDevice->handle, format, &props);

	if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
		return true;
	} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
		return true;
	}

	return false;
}

auto SwapChainResource::ChoosePresentMode(PresentMode const& desired_present_mode) -> PresentMode {
	for (auto const& mode : available_present_modes) {
		if (mode == static_cast<VkPresentModeKHR>(desired_present_mode)) {
			return desired_present_mode;
		}
	}
	VB_LOG_WARN("Preferred present mode not available!");
	// FIFO is guaranteed to be available
	return PresentMode::eFifo;
}

auto SwapChainResource::ChooseExtent(Extent2D const& desired_extent) -> Extent2D {
	if (this->surface_capabilities.currentExtent.width != UINT32_MAX) {
		return {
			surface_capabilities.currentExtent.width,
			surface_capabilities.currentExtent.height,
		};
	}
	return Extent2D {
		.width = std::max(
			this->surface_capabilities.minImageExtent.width,
			std::min(this->surface_capabilities.maxImageExtent.width, desired_extent.width)
		),
		.height = std::max(
			this->surface_capabilities.minImageExtent.height,
			std::min(this->surface_capabilities.maxImageExtent.height, desired_extent.height)
		)
	};
}

void SwapChainResource::CreateSurfaceFormats() {
	// get capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(owner->physicalDevice->handle, init_info.surface, &surface_capabilities);

	// get surface formats
	u32 formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(owner->physicalDevice->handle, init_info.surface, &formatCount, nullptr);
	if (formatCount != 0) {
		available_surface_formats.clear();
		available_surface_formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(owner->physicalDevice->handle, init_info.surface, &formatCount, available_surface_formats.data());
	}

	// get present modes
	u32 modeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(owner->physicalDevice->handle, init_info.surface, &modeCount, nullptr);
	if (modeCount != 0) {
		available_present_modes.clear();
		available_present_modes.resize(modeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(owner->physicalDevice->handle, init_info.surface, &modeCount, available_present_modes.data());
	}

	// set this device as not suitable if no surface formats or present modes available
	bool suitable = (modeCount > 0);
	suitable &= (formatCount > 0);
	VB_ASSERT(suitable, "Selected device invalidated after surface update.");
}

auto SwapChainResource::ChooseSurfaceFormat(SurfaceFormat const& desired_surface_format) -> SurfaceFormat {
	for (auto const& availableFormat : available_surface_formats) {
		if (availableFormat.format == VkFormat(desired_surface_format.format) &&
			availableFormat.colorSpace == VkColorSpaceKHR(desired_surface_format.colorSpace)) {
			return {Format(availableFormat.format), ColorSpace(availableFormat.colorSpace)};
		}
	}
	VB_LOG_WARN("Preferred surface format not available!");
	return {Format(available_surface_formats[0].format), ColorSpace(available_surface_formats[0].colorSpace)};
}

void SwapChainResource::CreateSwapchain() {
	auto const& capabilities = surface_capabilities;

	// acquire additional images to prevent waiting for driver's internal operations
	u32 image_count = init_info.frames_in_flight + init_info.additional_images;

	if (image_count < capabilities.minImageCount) {
		VB_LOG_WARN("Querying less images %u than the necessary: %u", image_count, capabilities.minImageCount);
		image_count = capabilities.minImageCount;
	}

	// prevent exceeding the max image count
	if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
		VB_LOG_WARN("Querying more images than supported. imageCount set to maxImageCount.");
		image_count = capabilities.maxImageCount;
	}

	// Create swapchain
	VkSwapchainCreateInfoKHR createInfo {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = init_info.surface,
		.minImageCount = image_count,
		.imageFormat = VkFormat(init_info.preferred_format),
		.imageColorSpace = static_cast<VkColorSpaceKHR>(init_info.color_space),
		.imageExtent = {init_info.extent.width, init_info.extent.height},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT  // if we want to render to a separate image first to perform post-processing
						| VK_IMAGE_USAGE_TRANSFER_DST_BIT, // we should add this image usage
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, // don't support different graphics and present family
		.queueFamilyIndexCount = 0,     // only when imageSharingMode is VK_SHARING_MODE_CONCURRENT
		.pQueueFamilyIndices = nullptr, // only when imageSharingMode is VK_SHARING_MODE_CONCURRENT
		.preTransform = capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, // blend the images with other windows in the window system
		.presentMode = static_cast<VkPresentModeKHR>(init_info.present_mode),
		.clipped = true, // clip pixels behind our window
		.oldSwapchain = handle,
	};

	VB_VK_RESULT result = vkCreateSwapchainKHR(owner->handle, &createInfo, owner->owner->allocator, &handle);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create swap chain!");	
	vkDestroySwapchainKHR(owner->handle, createInfo.oldSwapchain, owner->owner->allocator);
}

void SwapChainResource::CreateImages() {
	u32 imageCount;
	vkGetSwapchainImagesKHR(owner->handle, handle, &imageCount, nullptr);
	VB_VLA(VkImage, vk_images, imageCount);
	vkGetSwapchainImagesKHR(owner->handle, handle, &imageCount, vk_images.data());

	// Create image views
	images.clear();
	images.reserve(vk_images.size());
	for (auto vk_image: vk_images) {
		images.emplace_back();
		auto& image = images.back();
		image.resource = MakeResource<ImageResource>(owner, "SwapChain Image " + std::to_string(images.size()));
		image.resource->fromSwapchain = true;
		image.resource->handle  = vk_image;
		image.resource->extent = {init_info.extent.width, init_info.extent.height, 1};
		image.resource->layout = ImageLayout::eUndefined;
		image.resource->aspect = Aspect::eColor;

		VkImageViewCreateInfo viewInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = vk_image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = VkFormat(init_info.preferred_format),
			.subresourceRange {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};

		VB_VK_RESULT result = vkCreateImageView(owner->handle, &viewInfo, owner->owner->allocator, &image.resource->view);
		VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create SwapChain image view!");

		// Add debug names
		if (owner->owner->init_info.validation_layers) {
			owner->SetDebugUtilsObjectName({
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VkObjectType::VK_OBJECT_TYPE_IMAGE,
				.objectHandle = reinterpret_cast<uint64_t>(image.resource->handle),
				.pObjectName = image.resource->name.data(),
			});
			owner->SetDebugUtilsObjectName({
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VkObjectType::VK_OBJECT_TYPE_IMAGE_VIEW,
				.objectHandle = reinterpret_cast<uint64_t>(image.resource->view),
				.pObjectName = (image.resource->name + " View").data(),
			});
		}
	}
}

void SwapChainResource::CreateSemaphores() {
	image_available_semaphores.resize(init_info.frames_in_flight);
	render_finished_semaphores.resize(init_info.frames_in_flight);

	VkSemaphoreCreateInfo semaphoreInfo{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	for (size_t i = 0; i < init_info.frames_in_flight; ++i) {
		VB_VK_RESULT
		result = vkCreateSemaphore(owner->handle, &semaphoreInfo, nullptr, &image_available_semaphores[i]);
		VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create semaphore!");
		result = vkCreateSemaphore(owner->handle, &semaphoreInfo, nullptr, &render_finished_semaphores[i]);
		VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create semaphore!");
	}
}

void SwapChainResource::CreateCommands(u32 queueFamilyindex) {
	commands.resize(init_info.frames_in_flight);
	for (auto& cmd: commands) {
		cmd.resource = MakeResource<CommandResource>(owner, "Swapchain Command Buffer");
		cmd.resource->Create(queueFamilyindex);
	}
}

void SwapChainResource::Create(SwapchainInfo const& info) {
	// this is a necessary hackery
	init_info.~SwapchainInfo();
	new(&init_info) SwapchainInfo(info);
	CreateSurfaceFormats();
	auto [format, colorSpace] = ChooseSurfaceFormat({
		init_info.preferred_format,
		init_info.color_space
	});
	init_info.preferred_format = format;
	init_info.color_space = colorSpace;
	init_info.present_mode = ChoosePresentMode(init_info.present_mode);
	extent = ChooseExtent(init_info.extent);
	CreateSwapchain();
	CreateImages();
	CreateSemaphores();
	CreateCommands(info.queueFamilyindex);
	current_frame = 0;
	current_image_index = 0;
	dirty = false;
	VB_LOG_TRACE("Created Swapchain");
}

void SwapChainResource::Free() {
	VB_LOG_TRACE("[ Free ] type = %s, name = %s", GetType(), GetName());
	for (auto& cmd: commands) {
		vkWaitForFences(owner->handle, 1, &cmd.resource->fence, VK_TRUE, std::numeric_limits<u32>::max());
	}

	for (auto& image : images) {
		vkDestroyImageView(owner->handle, image.resource->view, owner->owner->allocator);
	}
	images.clear();

	for (size_t i = 0; i < init_info.frames_in_flight; i++) {
		vkDestroySemaphore(owner->handle, image_available_semaphores[i], owner->owner->allocator);
		vkDestroySemaphore(owner->handle, render_finished_semaphores[i], owner->owner->allocator);
	}
	commands.clear();
	image_available_semaphores.clear();
	render_finished_semaphores.clear();
	available_present_modes.clear();
	available_surface_formats.clear();


	vkDestroySwapchainKHR(owner->handle, handle, owner->owner->allocator);

	if (init_info.destroy_surface) {
		vkDestroySurfaceKHR(owner->owner->handle, init_info.surface, owner->owner->allocator);
	}

	handle = VK_NULL_HANDLE;
}

void Swapchain::Recreate(u32 width, u32 height) {
	VB_ASSERT(width > 0 && height > 0, "Window size is 0, swapchain NOT to be recreated");
	for (auto& cmd: resource->commands) {
		vkWaitForFences(resource->owner->handle, 1, &cmd.resource->fence, VK_TRUE, std::numeric_limits<u32>::max());
	}

	for (auto& image : resource->images) {
		vkDestroyImageView(resource->owner->handle, image.resource->view, resource->owner->owner->allocator);
	}
	resource->images.clear();
	resource->CreateSurfaceFormats();
	resource->extent = resource->ChooseExtent({width, height});
	auto [format, colorSpace] = resource->ChooseSurfaceFormat({
		resource->init_info.preferred_format, 
		resource->init_info.color_space
	});
	resource->init_info.preferred_format = format;
	resource->init_info.color_space = colorSpace;
	resource->init_info.present_mode = resource->ChoosePresentMode(resource->init_info.present_mode);
	resource->CreateSwapchain();
	resource->CreateImages();
}

bool Swapchain::AcquireImage() {
	auto result = vkAcquireNextImageKHR(
		resource->owner->handle,
		resource->handle,
		UINT64_MAX,
		resource->GetImageAvailableSemaphore(resource->current_frame),
		VK_NULL_HANDLE,
		&resource->current_image_index
	);

	switch (result) {
	[[likely]]
	case VK_SUCCESS:
		return true;
	case VK_ERROR_OUT_OF_DATE_KHR:
		VB_LOG_WARN("AcquireImage: Out of date");
		resource->dirty = true;
		return false;
	case VK_SUBOPTIMAL_KHR:
		return false;
	default:
		VB_CHECK_VK_RESULT(resource->owner->owner->init_info.checkVkResult, result, "Failed to acquire swap chain image!");
		return false;
	}
}

auto Swapchain::GetFormat() -> Format {
	return resource->init_info.preferred_format;
}

bool Swapchain::GetDirty() {
	return resource->dirty;
}

auto Swapchain::GetCurrentImage() -> Image& {
	return resource->images[resource->current_image_index];
}

auto Swapchain::GetCommandBuffer() -> Command& {
	return resource->commands[resource->current_frame];
}
// EndCommandBuffer + vkQueuePresentKHR
void Swapchain::SubmitAndPresent(Queue const& submit, Queue const& present) {
	auto& currentFrame = resource->current_frame;
	auto& currentImageIndex = resource->current_image_index;
	auto& cmd = GetCommandBuffer();

	cmd.End();
	cmd.QueueSubmit( submit, {
		.waitSemaphoreInfos   = {{{.semaphore = Semaphore(resource->GetImageAvailableSemaphore(currentFrame))}}},
		.signalSemaphoreInfos = {{{.semaphore = Semaphore(resource->GetRenderFinishedSemaphore(currentFrame))}}}
	});

	VkSemaphore present_wait = resource->GetRenderFinishedSemaphore(currentFrame);
	VkPresentInfoKHR presentInfo{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1, // TODO(nm): pass with info
		.pWaitSemaphores = &present_wait,
		.swapchainCount = 1,
		.pSwapchains = &resource->handle,
		.pImageIndices = &currentImageIndex,
		.pResults = nullptr,
	};

	auto result = vkQueuePresentKHR(present.resource->handle, &presentInfo); // TODO(nm): use present queue

	switch (result) {
	case VK_SUCCESS: break;
	case VK_ERROR_OUT_OF_DATE_KHR:
	case VK_SUBOPTIMAL_KHR:
		VB_LOG_WARN("vkQueuePresentKHR: %s", StringFromVkResult(result));
		resource->dirty = true;
		return;
	default:
		VB_CHECK_VK_RESULT(resource->owner->owner->init_info.checkVkResult, result, "Failed to present swap chain image!"); 
		break;
	}

	currentFrame = (currentFrame + 1) % resource->init_info.frames_in_flight;

}

auto Device::CreateSwapchain(SwapchainInfo const& info) -> Swapchain {
	Swapchain swapchain;
	swapchain.resource = MakeResource<SwapChainResource>(resource.get(), "Swapchain");
	swapchain.resource->Create(info);
	return swapchain;
}

auto Device::CreateDescriptor(std::span<BindingInfo const> bindings) -> Descriptor {
	VB_ASSERT(bindings.size() > 0, "Descriptor must have at least one binding");
	Descriptor descriptor;
	descriptor.resource = MakeResource<DescriptorResource>(resource.get(), "Descriptor");
	descriptor.resource->Create(bindings);
	return descriptor;
}

auto Device::CreateCommand(u32 queueFamilyindex) -> Command {
	Command command;
	command.resource = MakeResource<CommandResource>(resource.get(), "Command Buffer");
	command.resource->Create(queueFamilyindex);
	return command;
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
	for (auto const& [_, info] : bindings) {
		VB_LOG_TRACE("%u: %u", info.type, info.binding);
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
	for (u32 i = 0; auto const& [_, binding] : bindings) {
		pool_sizes[i] = {static_cast<VkDescriptorType>(binding.type), binding.count};
		++i;
	}
	u32 pool_flags = std::any_of(bindings.begin(), bindings.end(),
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

	for (u32 i = 0; auto const& [_, binding] : bindings) {
		layout_bindings[i] = {
			.binding = binding.binding,
			.descriptorType = static_cast<VkDescriptorType>(binding.type),
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


void CommandResource::Create(u32 queueFamilyindex) {
	VkCommandPoolCreateInfo poolInfo {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = 0, // do not use VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
		.queueFamilyIndex = queueFamilyindex
	};

	VkCommandBufferAllocateInfo allocInfo {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	VB_VK_RESULT result = vkCreateCommandPool(owner->handle, &poolInfo, owner->owner->allocator, &pool);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create command pool!");

	allocInfo.commandPool = pool;
	result = vkAllocateCommandBuffers(owner->handle, &allocInfo, &handle);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to allocate command buffer!");

	VkFenceCreateInfo fenceInfo{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	result = vkCreateFence(owner->handle, &fenceInfo, owner->owner->allocator, &fence);
	VB_CHECK_VK_RESULT(owner->owner->init_info.checkVkResult, result, "Failed to create fence!");
}

bool Command::Copy(Buffer const& dst, void const* data, u32 size, u32 dstOfsset) {
	auto device = resource->owner;
	VB_LOG_TRACE("[ CmdCopy ] size: %u, offset: %u", size, device->stagingOffset);
	if (device->init_info.staging_buffer_size - device->stagingOffset < size) [[unlikely]] {
		VB_LOG_WARN("Not enough size in staging buffer to copy");
		return false;
	}
	vmaCopyMemoryToAllocation(resource->owner->vmaAllocator, data, device->staging.resource->allocation, device->stagingOffset, size);
	Copy(dst, device->staging, size, dstOfsset, device->stagingOffset);
	device->stagingOffset += size;
	return true;
}

void Command::Copy(Buffer const& dst, Buffer const& src, u32 size, u32 dstOffset, u32 srcOffset) {
	VkBufferCopy2 copyRegion{
		.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
		.pNext = nullptr,
		.srcOffset = srcOffset,
		.dstOffset = dstOffset,
		.size = size,
	};

	VkCopyBufferInfo2 copyBufferInfo{
		.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
		.srcBuffer = src.resource->handle,
		.dstBuffer = dst.resource->handle,
		.regionCount = 1,
		.pRegions = &copyRegion
	};

	vkCmdCopyBuffer2(resource->handle, &copyBufferInfo);
}

bool Command::Copy(Image const& dst, void const* data, u32 size) {
	auto device = resource->owner;
	VB_LOG_TRACE("[ CmdCopy ] size: %u, offset: %u", size, device->stagingOffset);
	if (device->init_info.staging_buffer_size - device->stagingOffset < size) [[unlikely]] {
		VB_LOG_WARN("Not enough size in staging buffer to copy");
		return false;
	}
	std::memcpy(device->stagingCpu + device->stagingOffset, data, size);
	Copy(dst, device->staging, device->stagingOffset);
	device->stagingOffset += size;
	return true;
}

void Command::Copy(Image const& dst, Buffer const& src, u32 srcOffset) {
	VB_ASSERT(!(dst.resource->aspect & Aspect::eDepth || dst.resource->aspect & Aspect::eStencil),
		"CmdCopy doesnt't support depth/stencil images");
	VkBufferImageCopy2 region{
		.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
		.pNext = nullptr,
		.bufferOffset = srcOffset,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.imageOffset = { 0, 0, 0 },
		.imageExtent = { dst.resource->extent.width, dst.resource->extent.height, 1 },
	};

	VkCopyBufferToImageInfo2 copyBufferToImageInfo{
		.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
		.srcBuffer = src.resource->handle,
		.dstImage = dst.resource->handle,
		.dstImageLayout = (VkImageLayout)dst.resource->layout,
		.regionCount = 1,
		.pRegions = &region
	};

	vkCmdCopyBufferToImage2(resource->handle, &copyBufferToImageInfo);
}

void Command::Copy(Buffer const& dst, Image const& src, u32 dstOffset, Offset3D imageOffset, Extent3D imageExtent) {
	VB_ASSERT(!(src.resource->aspect & Aspect::eDepth || src.resource->aspect & Aspect::eStencil),
		"CmdCopy doesn't support depth/stencil images");
	VkBufferImageCopy2 region{
		.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
		.bufferOffset = dstOffset,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.imageOffset = {imageOffset.x, imageOffset.y, imageOffset.z },
		.imageExtent = {imageExtent.width, imageExtent.height, imageExtent.depth },
	};

	VkCopyImageToBufferInfo2 copyInfo{
		.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
		.srcImage = src.resource->handle,
		.srcImageLayout = (VkImageLayout)src.resource->layout,
		.dstBuffer = dst.resource->handle,
		.regionCount = 1,
		.pRegions = &region,
	};
	vkCmdCopyImageToBuffer2(resource->handle, &copyInfo);
}

void Command::Barrier(Image const& img, ImageBarrier const& barrier) {
	VkImageSubresourceRange range = {
		.aspectMask = (VkImageAspectFlags)img.resource->aspect,
		.baseMipLevel = 0,
		.levelCount = VK_REMAINING_MIP_LEVELS,
		.baseArrayLayer = 0,
		.layerCount = VK_REMAINING_ARRAY_LAYERS,
	};

	VkImageMemoryBarrier2 barrier2 = {
		.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext               = nullptr,
		.srcStageMask        = (VkPipelineStageFlags2)  barrier.memoryBarrier.srcStageMask,
		.srcAccessMask       = (VkAccessFlags2)         barrier.memoryBarrier.srcAccessMask,
		.dstStageMask        = (VkPipelineStageFlags2)  barrier.memoryBarrier.dstStageMask,
		.dstAccessMask       = (VkAccessFlags2)         barrier.memoryBarrier.dstAccessMask,
		.oldLayout           = (VkImageLayout)(barrier.oldLayout == ImageLayout::eUndefined
									? img.resource->layout
									: barrier.oldLayout),
		.newLayout           = (VkImageLayout)(barrier.newLayout == ImageLayout::eUndefined
									? img.resource->layout
									: barrier.newLayout),
		.srcQueueFamilyIndex = barrier.srcQueueFamilyIndex,
		.dstQueueFamilyIndex = barrier.dstQueueFamilyIndex,
		.image               = img.resource->handle,
		.subresourceRange    = range
	};

	VkDependencyInfo dependency = {
		.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext                    = nullptr,
		.dependencyFlags          = 0,
		.memoryBarrierCount       = 0,
		.pMemoryBarriers          = nullptr,
		.bufferMemoryBarrierCount = 0,
		.pBufferMemoryBarriers    = nullptr,
		.imageMemoryBarrierCount  = 1,
		.pImageMemoryBarriers     = &barrier2
	};

	vkCmdPipelineBarrier2(resource->handle, &dependency);
	img.resource->layout = barrier.newLayout;
}

void Command::Barrier(Buffer const& buf, BufferBarrier const& barrier) {
	VkBufferMemoryBarrier2 barrier2 {
		.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.pNext               = nullptr,
		.srcStageMask        = (VkPipelineStageFlags2) barrier.memoryBarrier.srcStageMask,
		.srcAccessMask       = (VkAccessFlags2)        barrier.memoryBarrier.srcAccessMask,
		.dstStageMask        = (VkPipelineStageFlags2) barrier.memoryBarrier.dstStageMask,
		.dstAccessMask       = (VkAccessFlags2)        barrier.memoryBarrier.dstAccessMask,
		.srcQueueFamilyIndex =                         barrier.srcQueueFamilyIndex,
		.dstQueueFamilyIndex =                         barrier.dstQueueFamilyIndex,
		.buffer              =                         buf.resource->handle,
		.offset              = (VkDeviceSize)          barrier.offset,
		.size                = (VkDeviceSize)          barrier.size
	};

	VkDependencyInfo dependency = {
		.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext                    = nullptr,
		.dependencyFlags          = 0,
		.bufferMemoryBarrierCount = 1,
		.pBufferMemoryBarriers    = &barrier2,
	};
	vkCmdPipelineBarrier2(resource->handle, &dependency);
}

void Command::Barrier(MemoryBarrier const& barrier) {
	VkMemoryBarrier2 barrier2 = {
		.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.pNext         = nullptr,
		.srcStageMask  = (VkPipelineStageFlags2) barrier.srcStageMask,
		.srcAccessMask = (VkAccessFlags2)        barrier.srcAccessMask,
		.dstStageMask  = (VkPipelineStageFlags2) barrier.dstStageMask,
		.dstAccessMask = (VkAccessFlags2)        barrier.dstAccessMask
	};
	VkDependencyInfo dependency = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &barrier2,
	};auto ff = vb::BufferUsage::eStorageBuffer | vb::BufferUsage::eTransferDst;
	vkCmdPipelineBarrier2(resource->handle, &dependency);
}

void Command::ClearColorImage(Image const& img, ClearColorValue const& color) {
	VkClearColorValue clearColor{{color.float32[0], color.float32[1], color.float32[2], color.float32[3]}};
	VkImageSubresourceRange range = {
		.aspectMask = (VkImageAspectFlags)img.resource->aspect,
		.baseMipLevel = 0,
		.levelCount = VK_REMAINING_MIP_LEVELS,
		.baseArrayLayer = 0,
		.layerCount = VK_REMAINING_ARRAY_LAYERS,
	};

	vkCmdClearColorImage(resource->handle, img.resource->handle,
		(VkImageLayout)img.resource->layout, &clearColor, 1, &range);
}

void Command::Blit(BlitInfo const& info) {
	auto dst = info.dst;
	auto src = info.src;
	auto regions = info.regions;

	ImageBlit const fullRegions[] = {{
		{{0, 0, 0}, Offset3D(dst.resource->extent)},
		{{0, 0, 0}, Offset3D(src.resource->extent)}
	}};

	if (regions.empty()) {
		regions = fullRegions;
	}

	VB_VLA(VkImageBlit2, blitRegions, regions.size());
	for (auto i = 0; auto& region: regions) {
		blitRegions[i] = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
			.pNext = nullptr,
			.srcSubresource{
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel       = 0,
				.baseArrayLayer = 0,
				.layerCount     = 1,
			},
			.srcOffsets = {VkOffset3D(region.src.offset), VkOffset3D(region.src.extent)},
			.dstSubresource{
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel       = 0,
				.baseArrayLayer = 0,
				.layerCount     = 1,
			},
			.dstOffsets = {VkOffset3D(region.dst.offset), VkOffset3D(region.dst.extent)},
		};
		++i;
	}

	VkBlitImageInfo2 blitInfo {
		.sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
		.pNext          = nullptr,
		.srcImage       = src.resource->handle,
		.srcImageLayout = (VkImageLayout)src.resource->layout,
		.dstImage       = dst.resource->handle,
		.dstImageLayout = (VkImageLayout)dst.resource->layout,
		.regionCount    = static_cast<u32>(blitRegions.size()),
		.pRegions       = blitRegions.data(),
		.filter         = VkFilter(info.filter),
	};

	vkCmdBlitImage2(resource->handle, &blitInfo);
}

auto DeviceResource::CreateSampler(SamplerInfo const& info) -> VkSampler {
	VkSamplerCreateInfo samplerInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext  = nullptr,
		.flags  = 0,
		.magFilter = VkFilter(info.magFilter),
		.minFilter = VkFilter(info.minFilter),
		.mipmapMode = VkSamplerMipmapMode(info.mipmapMode),
		.addressModeU = VkSamplerAddressMode(info.wrapU),
		.addressModeV = VkSamplerAddressMode(info.wrapV),
		.addressModeW = VkSamplerAddressMode(info.wrapW),
		.mipLodBias = 0.0f,

		.anisotropyEnable = info.anisotropyEnable == false
						? VK_FALSE
						: physicalDevice->physicalFeatures2.features.samplerAnisotropy,
		.maxAnisotropy = info.maxAnisotropy,
		.compareEnable = info.compareEnable == false ? VK_FALSE : VK_TRUE,
		.compareOp = VkCompareOp(info.compareOp),

		.minLod = info.minLod,
		.maxLod = info.maxLod,

		.borderColor = VkBorderColor(info.borderColor),
		.unnormalizedCoordinates = info.unnormalizedCoordinates,
	};

	VkSampler sampler = VK_NULL_HANDLE;
	VB_VK_RESULT result = vkCreateSampler(handle, &samplerInfo, nullptr, &sampler);
	VB_CHECK_VK_RESULT(owner->init_info.checkVkResult, result, "Failed to create texture sampler!");

	return sampler;
}

auto DeviceResource::GetOrCreateSampler(SamplerInfo const& desc) -> VkSampler {
	auto sampler = samplers.find(desc);
	if (sampler == samplers.end()) [[unlikely]] {
		VkSampler sampler = CreateSampler(desc);
		samplers[desc] = sampler;
		return sampler;
	} else {
		return sampler->second;
	}
}

auto Swapchain::GetImGuiInfo() -> ImGuiInitInfo {
	return resource->GetImGuiInfo();
}

auto SwapChainResource::GetImGuiInfo() -> ImGuiInitInfo {
	ImGuiInitInfo init_info{
		.Instance            = owner->owner->handle,
		.PhysicalDevice      = owner->physicalDevice->handle,
		.Device              = owner->handle,
		.DescriptorPool      = owner->imguiDescriptorPool,
		.MinImageCount       = std::max(surface_capabilities.minImageCount, 2u),
		.ImageCount          = (u32)images.size(),
		.PipelineCache       = owner->pipelineCache,
		.UseDynamicRendering = true,
		.Allocator           = reinterpret_cast<vk::AllocationCallbacks const*>(owner->owner->allocator),
	};
	return init_info;
}

void CheckVkResultDefault(int result, char const* message) {
	if (result == VK_SUCCESS) [[likely]] {
		return;
	}
	// stderr macro is not accessible with std module
#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
	fprintf(stderr, "[VULKAN ERROR: %s] %s\n", StringFromVkResult(result), message);
#else
	std::cerr << "[VULKAN ERROR: " << StringFromVkResult(result) << "] " << message << '\n';
#endif
	if (result < 0) {
		std::abort();
	}
}

auto StringFromVkResult(int result) -> char const* {
	switch (result)
	{
	case VK_SUCCESS:
		return "VK_SUCCESS";
	case VK_NOT_READY:
		return "VK_NOT_READY";
	case VK_TIMEOUT:
		return "VK_TIMEOUT";
	case VK_EVENT_SET:
		return "VK_EVENT_SET";
	case VK_EVENT_RESET:
		return "VK_EVENT_RESET";
	case VK_INCOMPLETE:
		return "VK_INCOMPLETE";
	case VK_ERROR_OUT_OF_HOST_MEMORY:
		return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY:
		return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case VK_ERROR_INITIALIZATION_FAILED:
		return "VK_ERROR_INITIALIZATION_FAILED";
	case VK_ERROR_DEVICE_LOST:
		return "VK_ERROR_DEVICE_LOST";
	case VK_ERROR_MEMORY_MAP_FAILED:
		return "VK_ERROR_MEMORY_MAP_FAILED";
	case VK_ERROR_LAYER_NOT_PRESENT:
		return "VK_ERROR_LAYER_NOT_PRESENT";
	case VK_ERROR_EXTENSION_NOT_PRESENT:
		return "VK_ERROR_EXTENSION_NOT_PRESENT";
	case VK_ERROR_FEATURE_NOT_PRESENT:
		return "VK_ERROR_FEATURE_NOT_PRESENT";
	case VK_ERROR_INCOMPATIBLE_DRIVER:
		return "VK_ERROR_INCOMPATIBLE_DRIVER";
	case VK_ERROR_TOO_MANY_OBJECTS:
		return "VK_ERROR_TOO_MANY_OBJECTS";
	case VK_ERROR_FORMAT_NOT_SUPPORTED:
		return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	case VK_ERROR_FRAGMENTED_POOL:
		return "VK_ERROR_FRAGMENTED_POOL";
	case VK_ERROR_UNKNOWN:
		return "VK_ERROR_UNKNOWN";
	case VK_ERROR_OUT_OF_POOL_MEMORY:
		return "VK_ERROR_OUT_OF_POOL_MEMORY";
	case VK_ERROR_INVALID_EXTERNAL_HANDLE:
		return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
	case VK_ERROR_FRAGMENTATION:
		return "VK_ERROR_FRAGMENTATION";
	case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
		return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
	case VK_PIPELINE_COMPILE_REQUIRED:
		return "VK_PIPELINE_COMPILE_REQUIRED";
	case VK_ERROR_SURFACE_LOST_KHR:
		return "VK_ERROR_SURFACE_LOST_KHR";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
		return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
	case VK_SUBOPTIMAL_KHR:
		return "VK_SUBOPTIMAL_KHR";
	case VK_ERROR_OUT_OF_DATE_KHR:
		return "VK_ERROR_OUT_OF_DATE_KHR";
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
		return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
	case VK_ERROR_VALIDATION_FAILED_EXT:
		return "VK_ERROR_VALIDATION_FAILED_EXT";
	case VK_ERROR_INVALID_SHADER_NV:
		return "VK_ERROR_INVALID_SHADER_NV";
	case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
		return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
		return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
		return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
		return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
		return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
		return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
	case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
		return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
	case VK_ERROR_NOT_PERMITTED_KHR:
		return "VK_ERROR_NOT_PERMITTED_KHR";
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
		return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
	case VK_THREAD_IDLE_KHR:
		return "VK_THREAD_IDLE_KHR";
	case VK_THREAD_DONE_KHR:
		return "VK_THREAD_DONE_KHR";
	case VK_OPERATION_DEFERRED_KHR:
		return "VK_OPERATION_DEFERRED_KHR";
	case VK_OPERATION_NOT_DEFERRED_KHR:
		return "VK_OPERATION_NOT_DEFERRED_KHR";
	case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR:
		return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
	case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
		return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
	case VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT:
		return "VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT";
	case VK_RESULT_MAX_ENUM:
		return "VK_RESULT_MAX_ENUM";
	default:
		if (result < 0)
			return "VK_ERROR_<Unknown>";
		return "VK_<Unknown>";
	}
}

} // namespace VB_NAMESPACE