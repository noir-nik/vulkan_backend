cmake_minimum_required(VERSION 3.30)
project(vulkan_backend)

if ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
		AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "16.0.0")
	OR (MSVC AND MSVC_VERSION GREATER_EQUAL 1931)
	OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
		AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14.1.0"))
	set(VB_MODULES_AVAILABLE true)
else()
	set(VB_MODULES_AVAILABLE false)
endif()

set(CMAKE_CXX_STANDARD 20)
find_package(Vulkan REQUIRED)

set(INCLUDE_DIRS
	include
	deps/VulkanMemoryAllocator/include
)

# SRC
file(GLOB SOURCES "src/*.cpp")

# disable warning for vma
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	add_definitions(
		-Wno-nullability-completeness
	)
endif()

add_library(${PROJECT_NAME} ${SOURCES})
add_library(vulkan_backend::vulkan_backend ALIAS vulkan_backend)

target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)

if(VB_BUILD_CPP_MODULE)
	if(VB_MODULES_AVAILABLE)
		add_library(vulkan_backend_module)
		add_library(vulkan_backend::module ALIAS vulkan_backend_module)
		
		if (VB_USE_STD_MODULE)
			target_compile_features(vulkan_backend_module PRIVATE cxx_std_23 INTERFACE cxx_std_20)
		else()
			target_compile_features(vulkan_backend_module PUBLIC cxx_std_20)
		endif()

		target_sources(vulkan_backend_module PUBLIC
			FILE_SET CXX_MODULES
			BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
			FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan_backend.cppm
		)
		target_link_libraries(vulkan_backend_module PRIVATE vulkan_backend::vulkan_backend)
		target_include_directories(vulkan_backend_module PRIVATE ${INCLUDE_DIRS})

		target_compile_definitions(vulkan_backend_module PUBLIC "VB_USE_STD_MODULE=$<BOOL:${VB_USE_STD_MODULE}>")
	else()
		message(WARNING "VB_BUILD_CPP_MODULE is ON but compiler does not support moduules")
	endif()
endif()

# Examples
if (VB_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()
