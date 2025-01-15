#ifndef VULKAN_BACKEND_RESOURCE_PIPELINE_LIBRARY_HPP_
#define VULKAN_BACKEND_RESOURCE_PIPELINE_LIBRARY_HPP_

#ifndef VB_USE_STD_MODULE
#include <span>
#include <unordered_map>
#else
import std;
#endif

#include <vulkan/vulkan.h>

#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/interface/pipeline.hpp"
#include "../util.hpp"

namespace VB_NAMESPACE {
static inline auto operator==(Pipeline::Stage const& lhs, Pipeline::Stage const& rhs) -> bool {
	return lhs.stage == rhs.stage && lhs.source.data == rhs.source.data &&
		lhs.entry_point == rhs.entry_point && lhs.compile_options == rhs.compile_options;
}

struct PipelineLibrary {
	Pipeline CreatePipeline(PipelineInfo const& desc);
	void Free();

private:
	struct VertexInputInfo {
		std::span<vk::Format const> vertexAttributes;
		bool lineTopology = false;
	};

	struct PreRasterizationInfo {
		VkPipelineLayout layout = VK_NULL_HANDLE;
		bool lineTopology = false;
		CullModeFlags cullMode = CullMode::eNone;
		std::span<Pipeline::Stage const> stages;
	};

	struct FragmentShaderInfo {
		VkPipelineLayout layout = VK_NULL_HANDLE;
		SampleCount samples;
		std::span<Pipeline::Stage const> stages;
	};

	struct FragmentOutputInfo {
		VkPipelineLayout layout = VK_NULL_HANDLE;
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
			size_t hash = HashCombine(FormatSpanHash(info.colorFormats), reinterpret_cast<size_t>(info.layout));
			hash = HashCombine(hash, info.useDepth);
			hash = HashCombine(hash, static_cast<size_t>(info.depthFormat));
			hash = HashCombine(hash, static_cast<size_t>(info.samples));
			return hash;
		}

		auto operator()(FragmentOutputInfo const& a, FragmentOutputInfo const& b) const -> bool {
			return std::equal(a.colorFormats.begin(), a.colorFormats.end(), b.colorFormats.begin()) && 
				a.layout == b.layout && 
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
			hash = HashCombine(hash, reinterpret_cast<size_t>(info.layout));
			hash = HashCombine(hash, static_cast<size_t>(info.samples));
			return hash;
		}
		auto operator()(FragmentShaderInfo const& a, FragmentShaderInfo const& b) const -> bool {
			return a.stages.size() == b.stages.size() && 
				std::equal(a.stages.begin(), a.stages.end(), b.stages.begin()) && 
				a.layout == b.layout && 
				a.samples == b.samples;
		}
	};

	void CreateVertexInputInterface(VertexInputInfo const& info);
	void CreatePreRasterizationShaders(PreRasterizationInfo const& info);
	void CreateFragmentShader(FragmentShaderInfo const& info);
	void CreateFragmentOutputInterface(FragmentOutputInfo const& info);
	auto LinkPipeline(std::array<VkPipeline, 4> const& pipelines, VkPipelineLayout layout, bool link_time_optimization) -> VkPipeline;

	DeviceResource* device = nullptr;
	std::unordered_map<VertexInputInfo, VkPipeline,VertexInputHash, VertexInputHash> vertexInputInterfaces;
	std::unordered_map<PreRasterizationInfo, VkPipeline, PreRasterizationHash, PreRasterizationHash> preRasterizationShaders;
	std::unordered_map<FragmentOutputInfo, VkPipeline, FragmentOutputHash, FragmentOutputHash> fragmentOutputInterfaces;
	std::unordered_map<FragmentShaderInfo, VkPipeline, FragmentShaderHash, FragmentShaderHash> fragmentShaders;
	friend DeviceResource;
};
} // namespace VB_NAMESPACE
#endif // VULKAN_BACKEND_RESOURCE_PIPELINE_LIBRARY_HPP_
