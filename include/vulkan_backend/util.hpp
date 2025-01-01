#ifndef VULKAN_BACKEND_UTIL_HPP_
#define VULKAN_BACKEND_UTIL_HPP_

#ifdef VB_DISABLE_ASSERT
#    undef VB_ASSERT
#    define VB_ASSERT(condition, msg) (void(0))
#elif !defined VB_ASSERT
#    include <cassert>
#    define VB_ASSERT(condition, msg) assert(((condition) && (msg)))
#endif

#ifdef VB_USE_VLA
#define VB_VLA(type, name, count) \
	type name##_vla[count]; \
	std::span<type> name(name##_vla, count)
#else
#define VB_VLA(type, name, count) \
	std::vector<type> name##_vla(count); \
	std::span<type> name(name##_vla.data(), count)
#endif

#define VB_VK_RESULT [[maybe_unused]] VkResult

#ifdef VB_DISABLE_VK_RESULT_CHECK
#define VB_CHECK_VK_RESULT(result, message)
#else
#define VB_CHECK_VK_RESULT(func, result, message) \
	if (func) \
	func(result, message); \

#endif

#ifdef VB_UNSAFE_CAST
#define CAST(type, value) *reinterpret_cast<type const*>(&value)
#else
#define CAST(type, value) Cast::type(value)
#endif

#define NEWLINE "\n"

#ifndef VB_NO_LOG

#ifndef VB_LOG_TRACE
#define VB_LOG_TRACE(fmt, ...) \
	if (global_log_level <= LogLevel::Trace) \
	std::printf(fmt NEWLINE, ## __VA_ARGS__)
#endif

#ifndef VB_LOG_INFO
#define VB_LOG_INFO(fmt, ...) \
	if (global_log_level <= LogLevel::Info) \
	std::printf(fmt NEWLINE, ## __VA_ARGS__)
#endif

#ifndef VB_LOG_WARN
#define VB_LOG_WARN(fmt, ...) \
	if (global_log_level <= LogLevel::Warning) \
	std::printf(fmt NEWLINE, ## __VA_ARGS__)
#endif

#ifndef VB_LOG_ERROR
#define VB_LOG_ERROR(fmt, ...) \
	if (global_log_level <= LogLevel::Error) \
	std::printf(fmt NEWLINE, ## __VA_ARGS__)
#endif

#endif // !VB_NO_LOG

#endif // VULKAN_BACKEND_UTIL_HPP_
