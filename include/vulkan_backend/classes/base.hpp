#pragma once

#ifndef VB_USE_STD_MODULE
#include <memory>
#include <string>
#include <utility>
#elif defined(VB_DEV)
import std;
#endif

#include "vulkan_backend/classes/no_copy_no_move.hpp"
#include "vulkan_backend/config.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
template <typename OwnerType> class OwnerBase;
template <typename OwnerType> class ResourceBase;
template <typename OwnerType> struct ResourceDeleter;

template <typename T> class OwnerBase : NoCopy, public std::enable_shared_from_this<T> {
  public:
	virtual ~OwnerBase() = default;

  protected:
	OwnerBase() = default;
};

// Base class for all resources
// Owner is typed to access its data from derived classes
template <typename OwnerType> class ResourceBase : NoCopy {
  public:
	inline ResourceBase(std::shared_ptr<OwnerType> const& owner = nullptr) : owner(owner) {}
	ResourceBase(ResourceBase&& other) noexcept : owner(std::move(other.owner)) {}
	ResourceBase& operator=(ResourceBase&& other) noexcept {
		owner = std::exchange(other.owner, {});
		return *this;
	}
	virtual ~ResourceBase() = default;
	// This should usually be overriden in derived classes for debug purposes
	virtual auto GetResourceTypeName() const -> char const* { return "ResourceBase"; }
	auto		 GetOwner() const -> OwnerType* { return owner.get(); }

  protected:
	// Pointer to owner object to keep it alive
	std::shared_ptr<OwnerType> owner;

  private:
	// Called when resource or owner goes out of scope
	// Should not be called directly
	virtual void Free() = 0;
	friend struct ResourceDeleter<OwnerType>;
	friend OwnerType;
};

// Base class for named resources
struct Named {
	Named(std::string_view name = "") : name(name) {}
	auto GetName() const -> std::string_view { return name; }
	void SetName(std::string_view name) { this->name = name; }

  private:
	std::string name = "";
};
} // namespace VB_NAMESPACE
