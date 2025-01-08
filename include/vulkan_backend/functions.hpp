#ifndef VULKAN_BACKEND_FUNCTIONS_HPP_
#define VULKAN_BACKEND_FUNCTIONS_HPP_

#include "config.hpp"
#include "interface/instance.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
auto CreateInstance(InstanceInfo const& info) -> Instance;
} // namespace VB_NAMESPACE
#endif // VULKAN_BACKEND_FUNCTIONS_HPP_