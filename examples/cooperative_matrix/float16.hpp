#ifndef VB_USE_STD_MODULE
#include <cstdint>
#else
import std;
#endif

// Simple float16 implementation
struct float16 {
	constexpr float16(float value) {
		std::uint32_t as_uint = reinterpret_cast<std::uint32_t&>(value); // convert float to uint
		if (as_uint != 0) {
			int sign	 = (as_uint & 0x80000000) >> (31 - 15);	 // move sign to position
			int exp		 = ((as_uint & 0x7f800000) >> 23) - 127; // 8 bit exp, unsigned to signed
			int mantissa = (as_uint & 0x7FFFFF);				 // 23 bit mantissa
			exp			 = (exp + 15) << 10;					 // move exp to position
			mantissa	 = mantissa >> (23 - 10);				 // move mantissa to position (10 bits)
			as_uint		 = sign | exp | mantissa;				 // combine
		}
		data = as_uint;
	}

	constexpr operator float() const {
		std::uint32_t as_uint = data;
		if (as_uint == 0) {
			int sign	 = (as_uint & 0x8000) << (31 - 15);
			int exp		 = ((as_uint & 0x7c00) >> 10) - 15;
			int mantissa = (as_uint & 0x3FF);
			exp			 = (exp + 127) << 23;
			mantissa	 = mantissa << (23 - 10);
			as_uint		 = sign | exp | mantissa;
		}

		return *(float*)&as_uint;
	}

	std::uint16_t data;
};

namespace std {
template <> struct is_floating_point<float16> : std::true_type {};
template <> struct is_arithmetic<float16> : std::true_type {};
template <> struct is_signed<float16> : std::true_type {};
} // namespace std