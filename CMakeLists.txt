cmake_minimum_required(VERSION 3.30)

option(VB_USE_VULKAN_MODULE OFF)
option(VB_USE_STD_MODULE OFF)
option(VB_BUILD_CPP_MODULE OFF)

if(VB_USE_STD_MODULE)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_MODULE_STD 1)
else()
set(CMAKE_CXX_STANDARD 20)
endif()

project(vulkan_backend LANGUAGES CXX)

if ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
		AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "16.0.0")
	OR (MSVC AND MSVC_VERSION GREATER_EQUAL 1931)
	OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
		AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14.1.0"))
	set(VB_MODULES_AVAILABLE true)
else()
	set(VB_MODULES_AVAILABLE false)
endif()

if(VB_BUILD_CPP_MODULE AND NOT VB_MODULES_AVAILABLE)
	message(WARNING "VB_BUILD_CPP_MODULE is ON but compiler does not support modules")
	set(VB_BUILD_CPP_MODULE OFF)
endif()

if (VB_USE_VULKAN_MODULE AND NOT VB_MODULES_AVAILABLE)
	message(WARNING "VB_USE_VULKAN_MODULE is ON but compiler does not support modules")
	set(VB_USE_VULKAN_MODULE OFF)
endif()

find_package(Vulkan REQUIRED)

set(INCLUDE_DIRS
	include
	src
	src/resource
	deps/VulkanMemoryAllocator/include
)

# SRC
file(GLOB_RECURSE SOURCES "src/*.cpp")

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

set(VULKAN_HPP_DEFINITIONS
	VULKAN_HPP_NO_EXCEPTIONS
	VULKAN_HPP_RAII_NO_EXCEPTIONS
	VULKAN_HPP_NO_SMART_HANDLE
	VULKAN_HPP_NO_CONSTRUCTORS
	VULKAN_HPP_NO_UNION_CONSTRUCTORS
	VULKAN_HPP_NO_SPACESHIP_OPERATOR
)

target_compile_definitions(${PROJECT_NAME} PUBLIC ${VULKAN_HPP_DEFINITIONS})

if (${VB_USE_VULKAN_MODULE})
	add_definitions("-DVB_USE_VULKAN_MODULE")
endif()

if (${VB_USE_STD_MODULE})
	add_definitions("-DVB_USE_STD_MODULE")
endif()

if (${VB_BUILD_CPP_MODULE})
	add_definitions("-DVB_BUILD_CPP_MODULE")
endif()

if(VB_BUILD_CPP_MODULE)
	add_library(vulkan_backend_module)
	add_library(vulkan_backend::module ALIAS vulkan_backend_module)
	
	if (VB_USE_STD_MODULE OR VB_USE_VULKAN_MODULE)
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

	target_compile_definitions(vulkan_backend_module PUBLIC ${VULKAN_HPP_DEFINITIONS})
endif()

# Examples
if (VB_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

# Vulkan-hpp
if (${VB_USE_VULKAN_MODULE})

	if( ${Vulkan_VERSION} VERSION_LESS "1.3.256" )
	message( FATAL_ERROR "Minimum required Vulkan version for C++ modules is 1.3.256. "
		"Found ${Vulkan_VERSION}."
	)
	endif()

	add_library( VulkanHppModule )

	target_sources( VulkanHppModule PUBLIC
		FILE_SET CXX_MODULES
		BASE_DIRS ${Vulkan_INCLUDE_DIR}
		FILES ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm
	)

	target_compile_features( VulkanHppModule PUBLIC cxx_std_23 )
	target_link_libraries( VulkanHppModule PUBLIC Vulkan::Vulkan )

	target_link_libraries( ${PROJECT_NAME} PRIVATE VulkanHppModule )
	target_compile_definitions(VulkanHppModule PUBLIC ${VULKAN_HPP_DEFINITIONS})

endif()

# Example command for compiling with C++ modules and clang
# cmake . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS=-stdlib=libc++ -DVB_USE_STD_MODULE=ON -DVB_USE_VULKAN_MODULE=ON -DVB_BUILD_CPP_MODULE=ON -DVB_BUILD_EXAMPLES=ON