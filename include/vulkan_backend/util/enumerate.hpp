#pragma once

#ifndef VB_USE_STD_MODULE
#include <tuple>
#elif defined(VB_DEV)
import std;
#endif

#include "vulkan_backend/config.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
namespace util {
// Iterates over a range and returns a tuple of [index, value]
struct EnumerateStruct {
	template <typename T,
			typename IterType = decltype(std::begin(std::declval<T>()))>
	constexpr auto operator()(T&& iterable) const {
		struct Iterator {
			std::size_t i;
			IterType iter;
			bool operator!=(Iterator const& other) const { return iter != other.iter; }
			void operator++() { ++i; ++iter; }
			auto operator*() const { return std::tie(i, *iter); }
		};
		struct IterRange {
			T collection;

			auto begin() { return Iterator{0, std::begin(collection)}; }
			auto end() { return Iterator{0, std::end(collection)}; }
		};
		return IterRange{std::forward<T>(iterable)};
	}
};
constexpr inline EnumerateStruct enumerate{};

} // namespace util
} // namespace VB_NAMESPACE
