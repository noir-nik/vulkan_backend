if(${Vulkan_VERSION} VERSION_GREATER_EQUAL "1.3.300" )

set(EXAMPLE_NAME cooperative_matrix)

file(GLOB_RECURSE EXAMPLE_SOURCE_FILES "*.cpp")

add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE_FILES})

target_link_libraries(${EXAMPLE_NAME} vulkan_backend::vulkan_backend)

if(VB_BUILD_CPP_MODULE)
	target_link_libraries(${EXAMPLE_NAME} vulkan_backend::module)
endif()

if (${VB_USE_VULKAN_MODULE})
	target_link_libraries( ${EXAMPLE_NAME}  VulkanHppModule )
endif()

target_include_directories(${EXAMPLE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_command(TARGET ${EXAMPLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix.comp
    $<TARGET_FILE_DIR:${EXAMPLE_NAME}>
)

else()
message( WARNING "coompative_matrix example requires Vk_NV_cooperative_matrix2, which is only available in Vulkan >= 1.3.300")
endif()