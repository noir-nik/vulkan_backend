#include <cstdio> // stderr macro is not accessible with std module

#ifndef VB_USE_STD_MODULE
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/vk_result.hpp"

namespace VB_NAMESPACE {
void (&CheckVkResult)(vk::Result, char const*) = CheckVkResultDefault;

void CheckVkResultDefault(vk::Result result, char const* message) {
	if (result == vk::Result::eSuccess) [[likely]] {
		return;
	}
	fprintf(stderr, "[VULKAN ERROR: %s] %s\n", StringFromVkResult(result), message);
	if (int(result) < 0) {
		std::abort();
	}
}

auto StringFromVkResult(vk::Result result) -> char const* {
	switch (result) {
	[[likely]] case vk::Result::eSuccess:
		return "VK_SUCCESS";
	case vk::Result::eNotReady:
		return "VK_NOT_READY";
	case vk::Result::eTimeout:
		return "VK_TIMEOUT";
	case vk::Result::eEventSet:
		return "VK_EVENT_SET";
	case vk::Result::eEventReset:
		return "VK_EVENT_RESET";
	case vk::Result::eIncomplete:
		return "VK_INCOMPLETE";
	case vk::Result::eErrorOutOfHostMemory:
		return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case vk::Result::eErrorOutOfDeviceMemory:
		return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case vk::Result::eErrorInitializationFailed:
		return "VK_ERROR_INITIALIZATION_FAILED";
	case vk::Result::eErrorDeviceLost:
		return "VK_ERROR_DEVICE_LOST";
	case vk::Result::eErrorMemoryMapFailed:
		return "VK_ERROR_MEMORY_MAP_FAILED";
	case vk::Result::eErrorLayerNotPresent:
		return "VK_ERROR_LAYER_NOT_PRESENT";
	case vk::Result::eErrorExtensionNotPresent:
		return "VK_ERROR_EXTENSION_NOT_PRESENT";
	case vk::Result::eErrorFeatureNotPresent:
		return "VK_ERROR_FEATURE_NOT_PRESENT";
	case vk::Result::eErrorIncompatibleDriver:
		return "VK_ERROR_INCOMPATIBLE_DRIVER";
	case vk::Result::eErrorTooManyObjects:
		return "VK_ERROR_TOO_MANY_OBJECTS";
	case vk::Result::eErrorFormatNotSupported:
		return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	case vk::Result::eErrorFragmentedPool:
		return "VK_ERROR_FRAGMENTED_POOL";
	case vk::Result::eErrorUnknown:
		return "VK_ERROR_UNKNOWN";
	case vk::Result::eErrorOutOfPoolMemory:
		return "VK_ERROR_OUT_OF_POOL_MEMORY";
	case vk::Result::eErrorInvalidExternalHandle:
		return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
	case vk::Result::eErrorFragmentation:
		return "VK_ERROR_FRAGMENTATION";
	case vk::Result::eErrorInvalidOpaqueCaptureAddress:
		return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
	case vk::Result::ePipelineCompileRequired:
		return "VK_PIPELINE_COMPILE_REQUIRED";
	case vk::Result::eErrorSurfaceLostKHR:
		return "VK_ERROR_SURFACE_LOST_KHR";
	case vk::Result::eErrorNativeWindowInUseKHR:
		return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
	case vk::Result::eSuboptimalKHR:
		return "VK_SUBOPTIMAL_KHR";
	case vk::Result::eErrorOutOfDateKHR:
		return "VK_ERROR_OUT_OF_DATE_KHR";
	case vk::Result::eErrorIncompatibleDisplayKHR:
		return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
	case vk::Result::eErrorValidationFailedEXT:
		return "VK_ERROR_VALIDATION_FAILED_EXT";
	case vk::Result::eErrorInvalidShaderNV:
		return "VK_ERROR_INVALID_SHADER_NV";
	case vk::Result::eErrorImageUsageNotSupportedKHR:
		return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
	case vk::Result::eErrorVideoPictureLayoutNotSupportedKHR:
		return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
	case vk::Result::eErrorVideoProfileOperationNotSupportedKHR:
		return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
	case vk::Result::eErrorVideoProfileFormatNotSupportedKHR:
		return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
	case vk::Result::eErrorVideoProfileCodecNotSupportedKHR:
		return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
	case vk::Result::eErrorVideoStdVersionNotSupportedKHR:
		return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
	case vk::Result::eErrorInvalidDrmFormatModifierPlaneLayoutEXT:
		return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
	case vk::Result::eErrorNotPermittedEXT:
		return "VK_ERROR_NOT_PERMITTED_EXT";
	case vk::Result::eThreadIdleKHR:
		return "VK_THREAD_IDLE_KHR";
	case vk::Result::eThreadDoneKHR:
		return "VK_THREAD_DONE_KHR";
	case vk::Result::eOperationDeferredKHR:
		return "VK_OPERATION_DEFERRED_KHR";
	case vk::Result::eOperationNotDeferredKHR:
		return "VK_OPERATION_NOT_DEFERRED_KHR";
	case vk::Result::eErrorInvalidVideoStdParametersKHR:
		return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
	case vk::Result::eErrorCompressionExhaustedEXT:
		return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
	case vk::Result::eErrorIncompatibleShaderBinaryEXT:
		return "VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT";
	default:
		if (int(result) < 0)
			return "VK_ERROR_<Unknown>";
		return "VK_<Unknown>";
	}
}

} // namespace VB_NAMESPACE