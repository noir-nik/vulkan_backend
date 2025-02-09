#pragma once

#ifndef VB_USE_STD_MODULE
#include <utility>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/classes/no_copy_no_move.hpp"
#include "vulkan_backend/config.hpp"
#include "vulkan_backend/interface/descriptor/descriptor.hpp"
#include "vulkan_backend/fwd.hpp"
#include "vulkan_backend/types.hpp"
#include "vulkan_backend/macros.hpp"

// Base class for GPU resources that have index in bindless array
namespace VB_NAMESPACE {
class BindlessResourceBase : public NoCopy {
  public:
	u32 static constexpr inline kNullID = ~0u;
	// Get position in bindless array with respective binding
	inline auto GetResourceID() const -> u32 { return resourceID; }
	// Bindless resource ID is usually set on creation
	inline void SetResourceID(u32 id) { resourceID = id; }

	inline auto GetBinding() const -> u32 { return binding; }
	inline void SetBinding(u32 binding) { binding = binding; }

	inline auto GetDescriptor() const -> BindlessDescriptor* { return descriptor; }
	inline void SetDescriptor(BindlessDescriptor* descriptor) { this->descriptor = descriptor; }

	// Aquire resource ID from bindless array
	void Bind(BindlessDescriptor& descriptor, u32 binding) {
		VB_DEBUG_ASSERT(this->descriptor == nullptr, "BindlessResourceBase::Bind(): Descriptor is already bound!");
		this->descriptor = &descriptor;
		this->binding    = binding;
		resourceID       = descriptor.PopID(GetBinding());
	}
	void Rebind(BindlessDescriptor& descriptor, u32 binding) { Release(); Bind(descriptor, binding); }

	bool IsBound() const { return descriptor != nullptr; }

	// Release resource ID to bindless array
	void Release() {
		VB_DEBUG_ASSERT(descriptor != nullptr, "BindlessResourceBase::Release(): Descriptor is null!");
		descriptor->PushID(GetBinding(), GetResourceID());
		descriptor = nullptr;
		binding    = kNullID;
		resourceID = kNullID;
	}

	BindlessResourceBase() = default;

	BindlessResourceBase(BindlessResourceBase&& other) noexcept {
		descriptor = std::exchange(other.descriptor, nullptr);
		binding    = std::exchange(other.binding, kNullID);
		resourceID = std::exchange(other.resourceID, kNullID);
	}

	BindlessResourceBase& operator=(BindlessResourceBase&& other) noexcept {
		descriptor = std::exchange(other.descriptor, nullptr);
		binding    = std::exchange(other.binding, kNullID);
		resourceID = std::exchange(other.resourceID, kNullID);
		return *this;
	}

  private:
	BindlessDescriptor* descriptor = nullptr;
	u32                 binding    = kNullID;
	u32                 resourceID = kNullID;
};
} // namespace VB_NAMESPACE