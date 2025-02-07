#pragma once

#include "vulkan_backend/config.hpp"
#include "vulkan_backend/types.hpp"

// Base class for GPU resources that have index in bindless array
namespace VB_NAMESPACE {
class GpuResource {
public:
	u32 static constexpr inline kNullID = ~0u;
	// Get position in bindless array with respective binding
	inline auto GetResourceID() const -> u32 { return resourceID; }
	// Bindless resource ID is usually set on creation
	inline void SetResourceID(u32 id) { resourceID = id; }
private:
	// u32 binding = kNullID;
	u32 resourceID = kNullID;
};
} // namespace VB_NAMESPACE