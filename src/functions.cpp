#ifndef VB_USE_STD_MODULE
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/functions.hpp"
#include "vulkan_backend/interface/instance/instance.hpp"

namespace VB_NAMESPACE {
auto SetupStructureChain(std::span<vk::BaseOutStructure* const> structures) -> vk::BaseOutStructure& {
	if (structures.size() > 1) [[likely]] {
		for (std::size_t i = 0; i < structures.size() - 1; ++i) {
			structures[i]->pNext = structures[i + 1];
		}
	}
	return *structures.front();
}

void AppendStructureChain(vk::BaseOutStructure* const base_structure, vk::BaseOutStructure* const structure_to_append) {
	vk::BaseOutStructure* iter = base_structure;
	while (iter->pNext != nullptr) {
		iter = iter->pNext;
	}
	iter->pNext = structure_to_append;
}

void InsertStructureAfter(vk::BaseOutStructure* const base_structure, vk::BaseOutStructure* const structure_to_insert) {
	structure_to_insert->pNext = base_structure->pNext;
	base_structure->pNext = structure_to_insert;
}
} // namespace VB_NAMESPACE