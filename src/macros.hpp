#ifndef VULKAN_BACKEND_MACROS_HPP_
#define VULKAN_BACKEND_MACROS_HPP_

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
#define VB_CHECK_VK_RESULT(result, message) (void(0))
#else
#define VB_CHECK_VK_RESULT(func, result, message) \
	if (func) \
		func(vk::Result(result), message);
#endif

#define NEWLINE "\n"

#ifndef VB_NO_LOG

#ifndef VB_LOG_TRACE
#define VB_LOG_TRACE(fmt, ...) \
	SendMessage(GetInstance()->init_info.message_callback, LogLevel::Trace, fmt, ## __VA_ARGS__)
#endif

#ifndef VB_LOG_INFO
#define VB_LOG_INFO(fmt, ...) \
	SendMessage(GetInstance()->init_info.message_callback, LogLevel::Info, fmt, ## __VA_ARGS__)
#endif

#ifndef VB_LOG_WARN
#define VB_LOG_WARN(fmt, ...) \
	SendMessage(GetInstance()->init_info.message_callback, LogLevel::Warning, fmt, ## __VA_ARGS__)
#endif

#ifndef VB_LOG_ERROR
#define VB_LOG_ERROR(fmt, ...) \
	SendMessage(GetInstance()->init_info.message_callback, LogLevel::Error, fmt, ## __VA_ARGS__)
#endif

#endif // !VB_NO_LOG

#endif // VULKAN_BACKEND_MACROS_HPP_
