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

#define VB_ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

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

#define VB_LOG_TRACE(fmt, args...) \
	if (global_log_level <= LogLevel::Trace) \
	std::printf(fmt NEWLINE, ## args)

#define VB_LOG_INFO(fmt, args...) \
	if (global_log_level <= LogLevel::Info) \
	std::printf(fmt NEWLINE, ## args)

#define VB_LOG_WARN(fmt, args...) \
	if (global_log_level <= LogLevel::Warning) \
	std::printf(fmt NEWLINE, ## args)

#define VB_LOG_ERROR(fmt, args...) \
	if (global_log_level <= LogLevel::Error) \
	std::printf(fmt NEWLINE, ## args)

#endif // VULKAN_BACKEND_UTIL_HPP_
