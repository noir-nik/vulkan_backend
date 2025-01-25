#pragma once

#ifndef VB_USE_STD_MODULE
#include <string>
#include <utility>
#elif defined(VB_DEV)
import std;
#endif

#include "vulkan_backend/classes/no_copy_no_move.hpp"
#include "vulkan_backend/config.hpp"

VB_EXPORT
namespace VB_NAMESPACE {

// Base class for all resources
// Owner is typed to access its data from derived classes
template <typename OwnerType> class ResourceBase : NoCopy {
  public:
	inline ResourceBase(OwnerType* const owner = nullptr) : owner(owner) {}
	inline ResourceBase(ResourceBase&& other) : owner(other.owner) { other.owner = nullptr; }
	inline auto operator=(ResourceBase&& other) -> ResourceBase& {
		owner = std::exchange(other.owner, nullptr);
		return *this;
	}
	virtual ~ResourceBase() = default;
	// This should usually be overriden in derived classes for debug purposes
	virtual auto GetResourceTypeName() const -> char const* { return "ResourceBase"; }
	auto		 GetOwner() const -> OwnerType* { return *owner; }

  protected:
	// Pointer to owner object to keep it alive
	OwnerType* owner;

  private:
	// Called when resource or owner goes out of scope
	// Should not be called directly
	virtual void Free() = 0;
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
