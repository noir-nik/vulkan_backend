#pragma once

#ifndef VB_USE_STD_MODULE
#include <span>
#include <cstdio>
#else
import std;
#endif

#ifndef VB_BUILD_CPP_MODULE
#include <vulkan_backend/core.hpp>
#else
import vulkan_backend;
#endif

inline auto FormatBool(bool value) -> char const* { return value ? "true" : "false"; };

inline void PrintCooperativeMatrixProperties(std::span<vk::CooperativeMatrixPropertiesKHR const> const& properties) {
	std::printf("=== Cooperative Matrix Properties ===\n");
	for (auto [i, property] : vb::util::enumerate(properties)) {
		std::printf("%2llu: ", i);
		std::printf("MSize: %-5u ", property.MSize);
		std::printf("NSize: %-5u ", property.NSize);
		std::printf("KSize: %-5u ", property.KSize);
		std::printf("AType: %-10s ", vk::to_string(property.AType).c_str());
		std::printf("BType: %-10s ", vk::to_string(property.BType).c_str());
		std::printf("CType: %-10s ", vk::to_string(property.CType).c_str());
		std::printf("ResultType: %-10s ", vk::to_string(property.ResultType).c_str());
		std::printf("saturatingAccumulation: %-10s ", FormatBool(property.saturatingAccumulation));
		std::printf("scope: %s ", vk::to_string(property.scope).c_str());
		std::printf("\n");
	}
}

inline void PrintCooperativeMatrix2Properties(std::span<vk::CooperativeMatrixFlexibleDimensionsPropertiesNV const> const& properties) {
	std::printf("=== Cooperative Matrix 2 Properties ===\n");
	for (auto [i, property] : vb::util::enumerate(properties)) {
		std::printf("%2llu: ", i);
		std::printf("MGranularity: %-5u ", property.MGranularity);
		std::printf("NGranularity: %-5u ", property.NGranularity);
		std::printf("KGranularity: %-5u ", property.KGranularity);
		std::printf("AType: %-10s ", vk::to_string(property.AType).c_str());
		std::printf("BType: %-10s ", vk::to_string(property.BType).c_str());
		std::printf("CType: %-10s ", vk::to_string(property.CType).c_str());
		std::printf("ResultType: %-10s ", vk::to_string(property.ResultType).c_str());
		std::printf("saturatingAccumulation: %-10s ", FormatBool(property.saturatingAccumulation));
		std::printf("scope: %-10s ", vk::to_string(property.scope).c_str());
		std::printf("workgroupInvocations: %u ", property.workgroupInvocations);
		std::printf("\n");
	}
}