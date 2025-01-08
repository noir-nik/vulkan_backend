#ifndef VULKAN_BACKEND_IO_HPP_
#define VULKAN_BACKEND_IO_HPP_

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include <vulkan_backend/config.hpp>
#include <vulkan_backend/fwd.hpp>

VB_EXPORT
namespace VB_NAMESPACE {
enum class LogLevel {
	Trace,
	Info,
	Warning,
	Error,
	None,
};

void CheckVkResultDefault(Result result, char const* message);
void MessageCallbackDefault(LogLevel level, char const* message);
auto StringFromVkResult(Result result) -> char const*;
} // namespace VB_NAMESPACE

namespace VB_NAMESPACE {
void SendMessage(void (&callback)(LogLevel, char const*), LogLevel level, char const* format, ...);
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_IO_HPP_