#pragma once

#ifndef VB_USE_STD_MODULE
#include <span>
#include <unordered_map>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/classes/no_copy_no_move.hpp"
#include "vulkan_backend/interface/pipeline_library/info.hpp"
#include "vulkan_backend/interface/pipeline/pipeline.hpp"
#include "vulkan_backend/classes/base.hpp"
#include "vulkan_backend/util/hash.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
class PipelineLibrary : NoCopyNoMove, ResourceBase<Device> {
public:
	// auto CreatePipeline(GraphicsPipelineInfo const& info) -> Pipeline;
	void Free();

private:
	Device* device = nullptr;
	friend Device;
};
} // namespace VB_NAMESPACE
