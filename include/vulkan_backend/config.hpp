#ifndef VULKAN_BACKEND_CONFIG_HPP_
#define VULKAN_BACKEND_CONFIG_HPP_

/* 
	Possible options
*/

//	Include imgui headers and provide imgui functions
// #define VB_IMGUI

// Include glfw headers and provide glfw functions
// #define VB_GLFW

// Disable result checks of vk.. calls
// #define VB_DISABLE_VK_RESULT_CHECK

// Disable all assertions
// #define VB_DISABLE_ASSERT

// Enable unsafe cast to vulkan enums with reinterpret_cast
// #define VB_UNSAFE_CAST

// Use variable length arrays (VLA) to avoid small temporary heap allocations
// #define VB_USE_VLA

// Do not define VMA_IMPLEMENTATION macro
// #define VB_VMA_IMPLEMENTATION 0

#if !defined( VB_NAMESPACE )
#define VB_NAMESPACE vb
#endif

/* 
	Shaders compilation
*/

#if !defined( VB_MAX_COMPILE_STRING_SIZE )
#define VB_MAX_COMPILE_STRING_SIZE 1024
#endif

#if !defined( VB_MAX_PATH_SIZE )
#define VB_MAX_PATH_SIZE 256
#endif

/* 
	Definition for cpp module
*/

#if !defined( VB_EXPORT )
#define VB_EXPORT
#endif

#endif // VULKAN_BACKEND_CONFIG_HPP_