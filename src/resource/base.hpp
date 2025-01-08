#ifndef VULKAN_BACKEND_RESOURCE_BASE_HPP_
#define VULKAN_BACKEND_RESOURCE_BASE_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <memory>
#include <string>
#else
import std;
#endif

#include <vulkan_backend/config.hpp>
#include <vulkan_backend/interface/instance.hpp>

#include "common.hpp"
#include "../util.hpp"

namespace VB_NAMESPACE {

u32 constexpr NullRID = ~0;

template <typename Owner>
class ResourceBase : NoCopyNoMove {
public:
	Owner* owner;
	std::string name;
	ResourceBase(Owner* owner, std::string_view const name = "") : owner(owner), name(name) {}
	virtual ~ResourceBase() = default;
	virtual void Free() = 0;
	virtual auto GetType() -> char const* = 0;
	auto GetName() -> char const* {return name.data();};
};

template <typename Owner>
struct ResourceDeleter {
	std::weak_ptr<Owner> weak_owner;

	void operator()(ResourceBase<Owner>* ptr) {
		if (auto owner = weak_owner.lock()) {
			ptr->Free();
			owner->resources.erase(ptr);
		}
		delete ptr;
	}
};

// We could use std::allocate_shared to avoid double allocation in std::shared_ptr
// constructor but in that case control block would be in same allocation with Resource
// and memomy would not be freed upon destruction if a single weak_ptr exists,
// which is a totally valid approach with small structs
template<typename Res, typename Owner>
requires std::is_base_of_v<std::enable_shared_from_this<Owner>, Owner> && (!std::same_as<Res, Owner>)
auto MakeResource(Owner* owner_ptr, std::string_view const name = "") -> std::shared_ptr<Res> {
	// Create a new resource with custom deleter
	auto res =  std::shared_ptr<Res>(
		new Res(owner_ptr, name),
		ResourceDeleter{owner_ptr->weak_from_this()}
	);
	// Log verbose
	// VB_LOG_TRACE("[ MakeResource ] type = %s, name = \"%s\", no. = %zu owner = \"%s\"",
	// 	res->GetType(), name.data(), owner_ptr->resources.size(), owner_ptr->GetName());

	// Add to owners set of resources
	auto [_, success] = owner_ptr->resources.emplace(res.get());
	VB_ASSERT(success, "Resource already exists!"); // This should never happen
	return res;
};
} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_RESOURCE_BASE_HPP_