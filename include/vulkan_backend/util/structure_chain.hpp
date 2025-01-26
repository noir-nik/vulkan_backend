#pragma once

#ifndef VB_USE_STD_MODULE
#include <span>
#elif defined(VB_DEV)
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif defined(VB_DEV)
import vulkan_hpp;
#endif

#include "vulkan_backend/config.hpp"

VB_EXPORT
namespace VB_NAMESPACE {
// Chains structures. Does NOT modify last structure
// Returns first structure reference
auto SetupStructureChain(std::span<void* const> structures) -> vk::BaseOutStructure*;
// void SetupStructureChain(std::span<void* const> structures);

// Appends structure to the end of structure chain that contains base_structure
// Before: base_structure -> SOME_STRUCTURE_A -> SOME_STRUCTURE_B
// After:  base_structure -> SOME_STRUCTURE_A -> SOME_STRUCTURE_B -> structure_to_append
void AppendStructureChain(void* const base_structure, void* const structure_to_append);

// Makes base_structure point to structure_to_insert and structure_to_insert point to what base_structure used to point to
// Before: base_structure -> SOME_STRUCTURE
// After:  base_structure -> structure_to_insert -> SOME_STRUCTURE
void InsertStructureAfter(void* const base_structure, void* const structure_to_insert);
} // namespace VB_NAMESPACE
