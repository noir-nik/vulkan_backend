#pragma once

#ifndef VB_USE_STD_MODULE
#include <cstddef>
#include <span>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"
#include "vulkan_backend/interface/pipeline/info.hpp"
#include "vulkan_backend/interface/pipeline_library/info.hpp"

#if !defined( VB_HASH_COMBINE )
#define VB_HASH_COMBINE( seed, value ) \
      { \
          auto x = std::hash<typename std::decay<decltype(value)>::type>{}(value); \
          x = ((x >> 16) ^ x) * 0x45d9f3b; \
          x = ((x >> 16) ^ x) * 0x45d9f3b; \
          x = (x >> 16) ^ x; \
          seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2); \
      }
#endif

VB_EXPORT
namespace VB_NAMESPACE {
inline auto HashCombine(std::size_t hash, std::size_t x) -> std::size_t {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return hash ^ x + 0x9e3779b9 + (hash << 6) + (hash >> 2);
}
} // namespace VB_NAMESPACE

namespace std {
template<typename T>
struct hash<std::span<T>> {
	std::size_t operator()(std::span<T> const& span) const {
		std::size_t seed = 0;
		for (auto const& val : span) {
			VB_HASH_COMBINE(seed, val);
		}
		return seed;
	}
};

template<>
struct hash<vb::PipelineLayoutInfo> {
	std::size_t operator()(vb::PipelineLayoutInfo const& info) const {
		std::size_t seed = 0;
		VB_HASH_COMBINE(seed, info.descriptor_set_layouts);
		VB_HASH_COMBINE(seed, info.push_constant_ranges);
		return seed;
	}
};
template<>
struct hash<vb::PipelineStage> {
	std::size_t operator()(vb::PipelineStage const& stage) const {
		std::size_t seed = 0;
		VB_HASH_COMBINE(seed, stage.stage);
		VB_HASH_COMBINE(seed, std::hash<std::string_view>{}(stage.source.data));
		VB_HASH_COMBINE(seed, std::hash<std::string_view>{}(stage.entry_point));
		VB_HASH_COMBINE(seed, std::hash<std::string_view>{}(stage.compile_options));
		return seed;
	}
};

template<>
struct hash<vb::VertexInputInfo> {
	std::size_t operator()(vb::VertexInputInfo const& info) const {
		std::size_t seed = 0;
		VB_HASH_COMBINE(seed, info.vertex_attributes);
		VB_HASH_COMBINE(seed, info.input_assembly);
		return seed;
	}
};

template<>
struct hash<vb::PreRasterizationInfo> {
	std::size_t operator()(vb::PreRasterizationInfo const& info) const {
		std::size_t seed = 0;
		VB_HASH_COMBINE(seed, info.layout);
		VB_HASH_COMBINE(seed, info.dynamic_states);
		VB_HASH_COMBINE(seed, info.stages);
		VB_HASH_COMBINE(seed, info.tessellation);
		VB_HASH_COMBINE(seed, info.viewport);
		VB_HASH_COMBINE(seed, info.rasterization);
		return seed;
	}
};

template<>
struct hash<vb::FragmentShaderInfo> {
	std::size_t operator()(vb::FragmentShaderInfo const& info) const {
		std::size_t hash = 0;
		VB_HASH_COMBINE(hash, info.layout);
		VB_HASH_COMBINE(hash, info.stages);
		VB_HASH_COMBINE(hash, info.depth_stencil);
		return hash;
	}
};

template<>
struct hash<vb::FragmentOutputInfo> {
	std::size_t operator()(vb::FragmentOutputInfo const& info) const {
		std::size_t seed = 0;
		VB_HASH_COMBINE(seed, info.blend_attachments);
		VB_HASH_COMBINE(seed, info.color_blend);
		VB_HASH_COMBINE(seed, info.multisample);
		VB_HASH_COMBINE(seed, info.dynamic_states);
		VB_HASH_COMBINE(seed, info.color_formats);
		VB_HASH_COMBINE(seed, info.depth_format);
		VB_HASH_COMBINE(seed, info.stencil_format);
		return seed;
	}
};

} // namespace std

