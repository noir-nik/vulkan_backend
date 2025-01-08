
#include "resource/descriptor.hpp"
#include "vulkan_backend/interface/descriptor.hpp"

namespace VB_NAMESPACE {
auto Descriptor::GetBinding(DescriptorType type) -> u32 {
	return resource->GetBinding(type);
}
}; // namespace VB_NAMESPACE