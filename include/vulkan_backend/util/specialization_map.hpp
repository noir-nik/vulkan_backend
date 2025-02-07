#pragma once

#ifndef VB_USE_STD_MODULE
#include <cstdint>
#include <array>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"

namespace VB_NAMESPACE {
namespace util {
// Helper struct to generate the entries array
template <typename T, std::size_t N, typename IndexSequence = std::make_index_sequence<N>>
struct SpecializationMapEntryHelper;

// Specialization of the helper struct to generate the entries
template <typename T, std::size_t N, std::size_t... Indices>
struct SpecializationMapEntryHelper<T, N, std::index_sequence<Indices...>> {
    static constexpr std::array<vk::SpecializationMapEntry, N> entries = {{
		{Indices, sizeof(T) * Indices, sizeof(T)}...
    }};
};
} // namespace util

VB_EXPORT
namespace util {
// Main SpecializationMapEntries struct
template <typename T, std::size_t N>
struct SpecializationMapEntries {
    static constexpr std::array<vk::SpecializationMapEntry, N> entries =
        SpecializationMapEntryHelper<T, N>::entries;
};

template <typename T, std::size_t N>
constexpr inline auto MakeSpecializationMapEntries() -> std::array<vk::SpecializationMapEntry, N> const& {
	return SpecializationMapEntries<T, N>::entries;
}

template <typename T, std::size_t N>
constexpr inline auto MakeSpecializationMapEntries(T const (&)[N]) -> std::array<vk::SpecializationMapEntry, N> const& {
	return SpecializationMapEntries<T, N>::entries;
}

template <typename T, std::size_t N>
constexpr inline auto MakeSpecializationInfo(T const (&specData)[N]) -> vk::SpecializationInfo {
	return vk::SpecializationInfo{
		.mapEntryCount = static_cast<decltype(vk::SpecializationInfo::mapEntryCount)>(std::size(specData)),
		.pMapEntries   = MakeSpecializationMapEntries(specData).data(),
		.dataSize	   = sizeof(specData),
		.pData		   = specData,
	};
}

template <typename T, std::size_t N>
constexpr inline auto MakeSpecializationInfo(std::array<T, N> const& specData) -> vk::SpecializationInfo {
	return vk::SpecializationInfo{
		.mapEntryCount = static_cast<decltype(vk::SpecializationInfo::mapEntryCount)>(std::size(specData)),
		.pMapEntries   = MakeSpecializationMapEntries<T, N>().data(),
		.dataSize	   = sizeof(specData),
		.pData		   = specData.data(),
	};
}

} // namespace util
} // namespace VB_NAMESPACE