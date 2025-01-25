#pragma once

#ifndef VB_USE_STD_MODULE
#include <cstdint>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
namespace algo {
inline bool SupportsExtension(std::span<vk::ExtensionProperties const> available_extensions,
							  std::string_view						   extension) {
	return std::any_of(available_extensions.begin(), available_extensions.end(),
					   [&extension](vk::ExtensionProperties const& available_extension) {
						   return available_extension.extensionName == extension;
					   });
};

inline bool SupportsExtensions(std::span<vk::ExtensionProperties const> available_extensions,
							   std::span<char const* const>				extensions) {
	return std::all_of(
		extensions.begin(), extensions.end(), [available_extensions](std::string_view extension) {
			return std::any_of(available_extensions.begin(), available_extensions.end(),
							   [&extension](vk::ExtensionProperties const& available_extension) {
								   return available_extension.extensionName == extension;
							   });
		});
}

inline bool SpanContains(std::span<char const* const> span, std::string_view value) {
	return std::find(span.begin(), span.end(), value) != span.end();
}

// template <typename T>
// inline bool SpanEqual(std::span<T const> lhs, std::span<T const> rhs) {
// 	return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
// };

template <typename T>
inline bool IsSpanEqual(std::span<T const> lhs, std::span<T const> rhs) {
	if (lhs.size() != rhs.size()) {
		return false;
	}
	for (std::size_t i = 0; i < lhs.size(); ++i) {
		if (lhs[i] != rhs[i]) {
			return false;
		}
	}
	return true;
}

template <typename T>
void Replace(std::span<T> buffer, T const& old_value, T const& new_value) {
	std::replace(buffer.begin(), buffer.end(), old_value, new_value);
}

} // namespace algo

template <typename T>
bool operator==(std::span<T const> const& lhs, std::span<T const> const& rhs) {
	return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

} // namespace VB_NAMESPACE
