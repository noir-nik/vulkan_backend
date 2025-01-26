module;

export module vulkan_backend;

#ifndef VB_USE_STD_MODULE
#define VB_USE_STD_MODULE 0
#endif

#ifndef VB_USE_VULKAN_MODULE
#define VB_USE_VULKAN_MODULE 0
#endif

#if VB_USE_STD_MODULE
import std;
#endif

#if VB_USE_VULKAN_MODULE
export import vulkan_hpp;
#endif

#ifdef VB_DEV
#undef VB_DEV
#endif

#define VB_EXPORT export
extern "C++" {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winclude-angled-in-module-purview"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 5244)
#endif

#if VB_USE_STD_MODULE
#include <cassert>
#endif

#include "vulkan_backend/core.hpp"

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
}