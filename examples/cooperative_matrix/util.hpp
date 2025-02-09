#pragma once

#ifndef VB_USE_STD_MODULE
#include <span>
#include <cstdio>
#include <cstdint>
#else
import std;
#endif

#ifndef VB_BUILD_CPP_MODULE
#include <vulkan_backend/core.hpp>
#else
import vulkan_backend;
#endif

#include "float16_t.hpp"
#include "matrix.hpp"

inline auto FormatBool(bool value) -> char const* { return value ? "true" : "false"; };

using numeric::float16_t;

inline void PrintCooperativeMatrixProperties(vk::CooperativeMatrixPropertiesKHR const& property) {
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

inline void PrintCooperativeMatrixProperties(std::span<vk::CooperativeMatrixPropertiesKHR const> properties) {
	std::printf("=== Cooperative Matrix Properties ===\n");
	for (auto [i, property] : vb::util::enumerate(properties)) {
		std::printf("%2llu: ", i);
		PrintCooperativeMatrixProperties(property);
	}
}

inline void PrintCooperativeMatrix2Property(vk::CooperativeMatrixFlexibleDimensionsPropertiesNV const& property) {
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
inline void PrintCooperativeMatrix2Properties(std::span<vk::CooperativeMatrixFlexibleDimensionsPropertiesNV const> properties) {
	std::printf("=== Cooperative Matrix 2 Properties ===\n");
	for (auto [i, property] : vb::util::enumerate(properties)) {
		std::printf("%2llu: ", i);
		PrintCooperativeMatrix2Property(property);
	}
}

template <typename T>
inline void PrintElement(T const value) {
	if constexpr (std::is_floating_point_v<T>) {
		std::printf("%f", float(value));
	} else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
		std::printf("%lld", int64_t(value));
	} else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
		std::printf("%llu", uint64_t(value));
	}
}

template <typename T>
inline void PrintMatrix(Matrix<T> const& matrix) {
	static constexpr uint32_t kMaxPrintRows = 10;
	static constexpr uint32_t kMaxPrintCols = 10;
	uint32_t rows_to_print = std::min(matrix.rows, kMaxPrintRows);
	uint32_t cols_to_print = std::min(matrix.cols, kMaxPrintCols);
	std::printf("[");
	for (uint32_t i = 0; i < rows_to_print; ++i) {			 
		std::printf("[");
		for (uint32_t j = 0; j < cols_to_print; ++j) {
			PrintElement(matrix(i, j));
			std::printf(" ");
		}
		if (matrix.cols > kMaxPrintCols) {
			std::printf("...");
		}
		std::printf("]\n");
	}
	if (matrix.rows > kMaxPrintRows) {
		std::printf("...]\n");
	} else {
		std::printf("}\n");
	}
}

template <typename AType, typename ResultType>
	requires MatrixType<AType> && MatrixType<ResultType>
constexpr inline size_t GetOptimalTileK() {
	if constexpr (std::same_as<AType, int8_t> || std::same_as<AType, uint8_t>) {
		return 64; // optimal if A and B element size is 8 bits
	} else if constexpr (std::is_same_v<ResultType, float16_t>) {
		return 32; // optimal if C and Result element type is float16_t
	} else {
		return 16;
	}
}

template <typename AType, typename ResultType>
	requires MatrixType<AType> && MatrixType<ResultType>
constexpr inline size_t GetOptimalTileK(Matrix<AType> const& matA, Matrix<ResultType> const& matC) {
	return GetOptimalTileK<AType, ResultType>();
}

template <typename AType>
inline void InitMatrixAB(Matrix<AType>& matrix) {
	if constexpr (std::is_floating_point_v<AType>) {
		for (uint32_t i = 0; i < matrix.GetTotalElements(); ++i) {
			matrix(i) = (static_cast<float>(std::rand() & 0x3) - 1.0f) / 2.0f;
		}
	} else if constexpr (std::is_integral_v<AType>) {
		for (uint32_t i = 0; i < matrix.GetTotalElements(); ++i) {
			matrix(i) = (std::rand() & 0xFF) - 128;
		}
	} else {
		static_assert(false, "Unsupported type.");
	}
}
template <typename CType>
inline void InitMatrixC(Matrix<CType>& matrix) {
	for (uint32_t i = 0; i < matrix.GetTotalElements(); ++i) {
		matrix(i) = CType(1234);
	}
}

template <typename CType>
inline void InitMatrixD(Matrix<CType>& matrix) {
	for (uint32_t i = 0; i < matrix.GetTotalElements(); ++i) {
		matrix(i) = CType(0);
	}
}

template <typename T>
inline constexpr bool MatchFormat(vk::ComponentTypeKHR const component_type) {
	if constexpr (std::is_same_v<T, float16_t>) {
		return component_type == vk::ComponentTypeKHR::eFloat16;
	} else if constexpr (std::is_same_v<T, float>) {
		return component_type == vk::ComponentTypeKHR::eFloat32;
	} else if constexpr (std::is_same_v<T, int8_t>) {
		return component_type == vk::ComponentTypeKHR::eSint8;
	} else if constexpr (std::is_same_v<T, int16_t>) {
		return component_type == vk::ComponentTypeKHR::eSint16;
	} else if constexpr (std::is_same_v<T, int32_t>) {
		return component_type == vk::ComponentTypeKHR::eSint32;
	} else if constexpr (std::is_same_v<T, int64_t>) {
		return component_type == vk::ComponentTypeKHR::eSint64;
	} else if constexpr (std::is_same_v<T, uint8_t>) {
		return component_type == vk::ComponentTypeKHR::eUint8;
	} else if constexpr (std::is_same_v<T, uint16_t>) {
		return component_type == vk::ComponentTypeKHR::eUint16;
	} else if constexpr (std::is_same_v<T, uint32_t>) {
		return component_type == vk::ComponentTypeKHR::eUint32;
	} else if constexpr (std::is_same_v<T, uint64_t>) {
		return component_type == vk::ComponentTypeKHR::eUint64;
	}
	return false;
}

template <typename Atype, typename ResultType>
bool ValidateParameters(
	uint32_t const M, uint32_t const N, uint32_t const K, uint32_t const tileM, uint32_t const tileN,
	uint32_t const tileK, uint32_t const workgroupSize,
	std::span<vk::CooperativeMatrixFlexibleDimensionsPropertiesNV const> flexibleDimensionsProperties) {
	bool bIsValid = std::any_of(
		flexibleDimensionsProperties.begin(), flexibleDimensionsProperties.end(), [&](auto const& property) {
			bool bMatchTile = (tileM % property.MGranularity == 0) && (tileN % property.NGranularity == 0) &&
							  (tileK % property.KGranularity == 0);
			bool bMatchFormat =
				MatchFormat<Atype>(property.AType) && MatchFormat<ResultType>(property.ResultType);
			bool bMatchScope	   = property.scope == vk::ScopeKHR::eWorkgroup;
			bool bMatchInvocations = property.workgroupInvocations == workgroupSize;
			return bMatchTile && bMatchFormat && bMatchScope && bMatchInvocations;
		});
	bIsValid &= (M % tileM == 0) && (N % tileN == 0) && (K % tileK == 0);
	return bIsValid;
}

template <typename AType, typename ResultType>
[[nodiscard]] uint32_t ValidateMultiplication(Matrix<AType> const& matA, Matrix<AType> const& matB,
							Matrix<ResultType> const& matC, Matrix<ResultType> const& matD, ResultType const alpha,
							ResultType const beta) {
	uint32_t M	 = matA.rows;
	uint32_t N	 = matB.cols;
	uint32_t K	 = matA.cols;

	uint32_t error_count = 0;
	uint32_t kMaxPrintErrors = 3;
	for (uint32_t i = 0; i < M; ++i) {
		for (uint32_t j = 0; j < N; ++j) {
			ResultType accumulator{};
			for (uint32_t k = 0; k < K; ++k) {
				accumulator += ResultType(matA(i, k) * matB(k, j));
			}
			accumulator = alpha * accumulator + beta * matC(i, j);
			ResultType d = matD(i, j);
			if (accumulator != d) [[unlikely]] {
				if (error_count < kMaxPrintErrors) {
					std::printf("error in [%d %d]: ", i, j);
					PrintElement(accumulator);
					std::printf(" != ");
					PrintElement(d);
					std::printf("\n");
				}
				++error_count;
			}
		}
	}
	return error_count;
}

template <typename T>
constexpr inline auto TypeToString() -> char const* {
	if constexpr (std::same_as<T, std::int8_t>) {
		return "int8_t";
	} else if constexpr (std::same_as<T, std::int16_t>) {
		return "int16_t";
	} else if constexpr (std::same_as<T, std::int32_t>) {
		return "int32_t";
	} else if constexpr (std::same_as<T, std::int64_t>) {
		return "int64_t";
	} else if constexpr (std::same_as<T, std::uint8_t>) {
		return "uint8_t";
	} else if constexpr (std::same_as<T, std::uint16_t>) {
		return "uint16_t";
	} else if constexpr (std::same_as<T, std::uint32_t>) {
		return "uint32_t";
	} else if constexpr (std::same_as<T, std::uint64_t>) {
		return "uint64_t";
	} else if constexpr (std::same_as<T, float>) {
		return "float";
	} else if constexpr (std::same_as<T, float16_t>) {
		return "float16_t";
	}
};

template <typename T> constexpr auto CastTo32Bit(T value) {
	if constexpr (std::is_floating_point_v<T>) {
		return static_cast<float>(value);
	} else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
		return static_cast<int32_t>(value);
	} else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
		return static_cast<uint32_t>(value);
	} else {
		static_assert(false, "Unsupported type.");
	}
}