#pragma once

#ifdef VB_DISABLE_ASSERT
#    undef VB_ASSERT
#    define VB_ASSERT(condition, msg) (void(0))
#elif !defined VB_ASSERT
#    include <cassert>
#    define VB_ASSERT(condition, msg) assert(((condition) && (msg)))
#endif

#ifdef VB_USE_VLA
#define VB_VLA(type, name, count) \
	std::size_t name##_vla_size = count > 0 ? count : 1; \
	type name##_vla[name##_vla_size]; \
	std::span<type> name(name##_vla, name##_vla_size)
#else
#define VB_VLA(type, name, count) \
	std::vector<type> name##_vla(count); \
	std::span<type> name(name##_vla.data(), count)
#endif // VB_USE_VLA

#define VB_FORMAT_TO_VLA(buffer_name, format, ...) \
	VB_VLA(char, buffer_name, std::snprintf(nullptr, 0, format, __VA_ARGS__) + 1); \
	int buffer_name##_strlen = std::snprintf(buffer_name.data(), buffer_name.size(), format, __VA_ARGS__);

#define VB_FORMAT_TO_BUFFER(buffer_name, format, ...) \
	std::snprintf(buffer_name, std::size(buffer_name) - 1, format, __VA_ARGS__);

#define VB_FORMAT_TO_BUFFER_GET_LENGTH(buffer_name, formated_size_result, format, ...) \
	formated_size_result = std::snprintf(buffer_name, std::size(buffer_name) - 1, format, __VA_ARGS__);

#define VB_FORMAT_TO_VLA_WITH_SIZE(buffer_name, dynamic_size, format, ...) \
	VB_VLA(char, buffer_name, dynamic_size + 1); \
	int buffer_name##_strlen = std::snprintf(buffer_name.data(), dynamic_size, format, __VA_ARGS__); \

#define VB_FORMAT_WITH_SIZE(buffer_name, compile_time_size, format, ...) \
	char buffer_name[compile_time_size + 1]; \
	std::snprintf(buffer_name, compile_time_size, format, __VA_ARGS__);

#define VB_FORMAT_WITH_SIZE_GET_LENGTH(buffer_name, compile_time_size, formated_length, format, ...) \
	char buffer_name[compile_time_size + 1]; \
	int formated_length = std::snprintf(buffer_name, compile_time_size, format, __VA_ARGS__);


#define NEWLINE "\n"

