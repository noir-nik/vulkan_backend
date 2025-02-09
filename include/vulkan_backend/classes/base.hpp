#pragma once
#include <cassert>

#ifndef VB_USE_STD_MODULE
#include <string>
#include <utility>
#elif defined(VB_DEV)
import std;
#endif

#include "vulkan_backend/classes/no_copy_no_move.hpp"
#include "vulkan_backend/config.hpp"
#include "vulkan_backend/macros.hpp"

VB_EXPORT
namespace VB_NAMESPACE {

// Base class for all resources
// Owner is typed to access its data from derived classes
template <typename OwnerType> class ResourceBase : NoCopy {
  public:
	inline ResourceBase(OwnerType* const owner = nullptr) : owner(owner) {}
	virtual ~ResourceBase() = default;
	
	inline ResourceBase(ResourceBase&& other) : owner(other.owner) { other.owner = nullptr; }
	inline auto operator=(ResourceBase&& other) -> ResourceBase& {
		owner = std::exchange(other.owner, nullptr);
		return *this;
	}

	auto GetOwner() const -> OwnerType* { return owner; }

	// This should usually be overriden in derived classes for debug purposes
	virtual auto GetResourceTypeName() const -> char const* { return "ResourceBase"; }

  protected:
	// Manually set owner with Create() function
	void SetOwner(OwnerType* const owner) {
		VB_ASSERT(this->owner == nullptr, "Tried to set owner on resource that already has one");
		this->owner = owner;
	}
  private:
	// Called when resource or owner goes out of scope
	// Should not be called directly
	virtual void Free() = 0;
	OwnerType* owner = nullptr;
	friend OwnerType;
};

// Base class for named resources
struct Named {
	Named(std::string_view name = "") : name(name) {}
	auto GetName() const -> std::string_view { return name; }
	void SetName(std::string_view name) { this->name = name; }

  private:
	std::string name;
};
} // namespace VB_NAMESPACE
