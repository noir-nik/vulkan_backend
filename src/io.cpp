#include <cstdarg>

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <cstdio>
#else
import std.compat;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/io.hpp"

namespace VB_NAMESPACE {
int static constexpr kMessageBufferSize = 1024;

void CheckVkResultDefault(Result result, char const* message) {
	if (result == Result::eSuccess) [[likely]] {
		return;
	}
	// stderr macro is not accessible with std module
#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
	fprintf(stderr, "[VULKAN ERROR: %s] %s\n", StringFromVkResult(result), message);
#else
	std::cerr << "[VULKAN ERROR: " << StringFromVkResult(result) << "] " << message << '\n';
#endif
	if (int(result) < 0) {
		std::abort();
	}
}

void MessageCallbackDefault(LogLevel level, char const* message) {
	if (level < LogLevel::Info) [[likely]] {
		return;
	}
	std::printf("%s\n", message);
}

void SendMessage(void (&callback)(LogLevel, char const*), LogLevel level, char const* format, ...) {
	char buffer[kMessageBufferSize];
	va_list args;
	va_start(args, format);
	int size = vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	if (size < 0) [[unlikely]] {
		return;
	}
	buffer[sizeof(buffer) - 1] = '\0';
	callback(level, buffer);
}

auto StringFromVkResult(Result result) -> char const* {
	switch (result) {
	[[likely]] case Result::eSuccess:
		return "VK_SUCCESS";
	case Result::eNotReady:
		return "VK_NOT_READY";
	case Result::eTimeout:
		return "VK_TIMEOUT";
	case Result::eEventSet:
		return "VK_EVENT_SET";
	case Result::eEventReset:
		return "VK_EVENT_RESET";
	case Result::eIncomplete:
		return "VK_INCOMPLETE";
	case Result::eErrorOutOfHostMemory:
		return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case Result::eErrorOutOfDeviceMemory:
		return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case Result::eErrorInitializationFailed:
		return "VK_ERROR_INITIALIZATION_FAILED";
	case Result::eErrorDeviceLost:
		return "VK_ERROR_DEVICE_LOST";
	case Result::eErrorMemoryMapFailed:
		return "VK_ERROR_MEMORY_MAP_FAILED";
	case Result::eErrorLayerNotPresent:
		return "VK_ERROR_LAYER_NOT_PRESENT";
	case Result::eErrorExtensionNotPresent:
		return "VK_ERROR_EXTENSION_NOT_PRESENT";
	case Result::eErrorFeatureNotPresent:
		return "VK_ERROR_FEATURE_NOT_PRESENT";
	case Result::eErrorIncompatibleDriver:
		return "VK_ERROR_INCOMPATIBLE_DRIVER";
	case Result::eErrorTooManyObjects:
		return "VK_ERROR_TOO_MANY_OBJECTS";
	case Result::eErrorFormatNotSupported:
		return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	case Result::eErrorFragmentedPool:
		return "VK_ERROR_FRAGMENTED_POOL";
	case Result::eErrorUnknown:
		return "VK_ERROR_UNKNOWN";
	case Result::eErrorOutOfPoolMemory:
		return "VK_ERROR_OUT_OF_POOL_MEMORY";
	case Result::eErrorInvalidExternalHandle:
		return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
	case Result::eErrorFragmentation:
		return "VK_ERROR_FRAGMENTATION";
	case Result::eErrorInvalidOpaqueCaptureAddress:
		return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
	case Result::ePipelineCompileRequired:
		return "VK_PIPELINE_COMPILE_REQUIRED";
	case Result::eErrorSurfaceLostKHR:
		return "VK_ERROR_SURFACE_LOST_KHR";
	case Result::eErrorNativeWindowInUseKHR:
		return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
	case Result::eSuboptimalKHR:
		return "VK_SUBOPTIMAL_KHR";
	case Result::eErrorOutOfDateKHR:
		return "VK_ERROR_OUT_OF_DATE_KHR";
	case Result::eErrorIncompatibleDisplayKHR:
		return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
	case Result::eErrorValidationFailedEXT:
		return "VK_ERROR_VALIDATION_FAILED_EXT";
	case Result::eErrorInvalidShaderNV:
		return "VK_ERROR_INVALID_SHADER_NV";
	case Result::eErrorImageUsageNotSupportedKHR:
		return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
	case Result::eErrorVideoPictureLayoutNotSupportedKHR:
		return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
	case Result::eErrorVideoProfileOperationNotSupportedKHR:
		return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
	case Result::eErrorVideoProfileFormatNotSupportedKHR:
		return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
	case Result::eErrorVideoProfileCodecNotSupportedKHR:
		return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
	case Result::eErrorVideoStdVersionNotSupportedKHR:
		return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
	case Result::eErrorInvalidDrmFormatModifierPlaneLayoutEXT:
		return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
	case Result::eErrorNotPermittedEXT:
		return "VK_ERROR_NOT_PERMITTED_EXT";
	case Result::eThreadIdleKHR:
		return "VK_THREAD_IDLE_KHR";
	case Result::eThreadDoneKHR:
		return "VK_THREAD_DONE_KHR";
	case Result::eOperationDeferredKHR:
		return "VK_OPERATION_DEFERRED_KHR";
	case Result::eOperationNotDeferredKHR:
		return "VK_OPERATION_NOT_DEFERRED_KHR";
	case Result::eErrorInvalidVideoStdParametersKHR:
		return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
	case Result::eErrorCompressionExhaustedEXT:
		return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
	case Result::eErrorIncompatibleShaderBinaryEXT:
		return "VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT";
	default:
		if (int(result) < 0)
			return "VK_ERROR_<Unknown>";
		return "VK_<Unknown>";
	}
}

} // namespace VB_NAMESPACE