#pragma once

#ifndef VB_USE_STD_MODULE
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

using std::int8_t;
using std::int16_t;
using std::int32_t;
using std::int64_t;
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

using std::size_t;
using numeric::float16_t;

struct MatrixBase {
	uint32_t   rows, cols;
	void*	   data;
	vb::Buffer hostBuffer;
	vb::Buffer deviceBuffer;

	MatrixBase(vb::Device& device, uint32_t rows, uint32_t cols, uint32_t element_size)
		: rows{rows}, cols{cols},
		  // Buffers
		  hostBuffer(device,
					 {
						 .size	= rows * cols * element_size,
						 .usage = vk::BufferUsageFlagBits::eTransferSrc | // for matA, matB, matC
								  vk::BufferUsageFlagBits::eTransferDst,  // for matD
						 .memory = vb::Memory::eCPU,
					 }),
		  deviceBuffer(device, {
								   .size  = rows * cols * element_size,
								   .usage = vk::BufferUsageFlagBits::eStorageBuffer |
											vk::BufferUsageFlagBits::eTransferDst | // for matA, matB, matC
											vk::BufferUsageFlagBits::eTransferSrc | // for matD
											vk::BufferUsageFlagBits::eShaderDeviceAddress,
								   .memory = vb::Memory::eGPU,
							   }) {
		data = hostBuffer.GetMappedData();
	}
};

template <typename... Ts>
struct TypeIn {
	template <typename T>
	constexpr static bool value = (std::same_as<T, Ts> || ...);
};

// Some theoretically supported types from spec
template <typename T>
concept MatrixType = TypeIn<float16_t, float, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t,
							 uint32_t, uint64_t>::template value<T>;

template <typename T>
	requires MatrixType<T>
struct Matrix : public MatrixBase {
	using ValueType = T;
	Matrix(vb::Device& device, uint32_t rows, uint32_t cols) : MatrixBase(device, rows, cols, sizeof(T)) {}

	inline T& operator()(size_t m, size_t n) { return static_cast<T*>(data)[m * cols + n]; }
	inline T& operator()(size_t element) { return static_cast<T*>(data)[element]; }
	inline T const& operator()(size_t m, size_t n) const { return static_cast<T*>(data)[m * cols + n]; }
	inline T const& operator()(size_t element) const { return static_cast<T*>(data)[element]; }
	inline size_t GetElementSize() const { return sizeof(T); }
	inline size_t GetTotalElements() const { return rows * cols; }
	inline size_t GetSize() const { return GetTotalElements() * GetElementSize();}
		
	inline size_t RavelIndex(size_t row, size_t col) const {
		return row * cols + col;
	}

	inline std::pair<size_t, size_t> UnravelIndex(size_t index) const {
		size_t row = index / cols;
		size_t col = index % cols;
		return {row, col};
	}
};

// Matrix multiplication for MxNxK matrices
// alpha*A*B + beta*C = D
// A = MxK, B = KxN, C,D = MxN
template <typename AType, typename ResultType>
void MatMulAdd(Matrix<AType> const& matA, Matrix<AType> const& matB, Matrix<ResultType> const& matC,
			  Matrix<ResultType>& matD, ResultType const alpha, ResultType const beta) {
	uint32_t const M = matA.rows;
	uint32_t const N = matB.cols;
	uint32_t const K = matA.cols;

	for (uint32_t i = 0; i < M; ++i) {
		for (uint32_t j = 0; j < N; ++j) {
			ResultType accumulator{};
			for (uint32_t k = 0; k < K; ++k) {
				accumulator += ResultType(matA(i, k) * matB(k, j));
			}
			matD(i, j) = alpha * accumulator + beta * matC(i, j);
		}
	}
}
