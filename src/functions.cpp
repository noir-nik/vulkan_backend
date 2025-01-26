#ifndef VB_USE_STD_MODULE
#else
import std;
#endif

#ifndef VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "vulkan_backend/util/structure_chain.hpp"
#include "vulkan_backend/interface/instance/instance.hpp"

namespace VB_NAMESPACE {
auto SetupStructureChain(std::span<void* const> structures) -> vk::BaseOutStructure* {
	if (structures.size() > 1) [[likely]] {
		for (std::size_t i = 0; i < structures.size() - 1; ++i) {
			auto iter = reinterpret_cast<vk::BaseOutStructure*>(structures[i]);
			auto next = reinterpret_cast<vk::BaseOutStructure*>(structures[i + 1]);
			iter->pNext = next;
		}
	}
	return reinterpret_cast<vk::BaseOutStructure*>(structures.front());
}

void AppendStructureChain(void* const base_structure, void* const structure_to_append) {
	vk::BaseOutStructure* iter = reinterpret_cast<vk::BaseOutStructure*>(base_structure);
	while (iter->pNext != nullptr) {
		iter = iter->pNext;
	}
	iter->pNext = reinterpret_cast<vk::BaseOutStructure*>(structure_to_append);
}

void InsertStructureAfter(void* const base_structure, void* const structure_to_insert) {
	auto base = reinterpret_cast<vk::BaseOutStructure*>(base_structure);
	auto insert = reinterpret_cast<vk::BaseOutStructure*>(structure_to_insert);
	insert->pNext = base->pNext;
	base->pNext = insert;
}
} // namespace VB_NAMESPACE