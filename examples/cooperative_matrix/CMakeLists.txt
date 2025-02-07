if(${Vulkan_VERSION} VERSION_GREATER_EQUAL "1.3.300" )
find_program(glslc HINTS Vulkan::glslc)
if (${Vulkan_glslc_FOUND})

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

set(SHADER_SRC_NAME cooperative_matrix)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix_f16_f32.comp.spv ${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix_f16_f16.comp.spv ${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix_i8_i32.comp.spv ${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix_u8_u32.comp.spv
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix.comp -fentry-point=main --target-spv=spv1.6 -DA_TYPE=float16_t -DC_TYPE=float     -o ${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix_f16_f32.comp.spv
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix.comp -fentry-point=main --target-spv=spv1.6 -DA_TYPE=float16_t -DC_TYPE=float16_t -o ${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix_f16_f16.comp.spv
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix.comp -fentry-point=main --target-spv=spv1.6 -DA_TYPE=int8_t    -DC_TYPE=int32_t   -o ${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix_i8_i32.comp.spv
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix.comp -fentry-point=main --target-spv=spv1.6 -DA_TYPE=uint8_t   -DC_TYPE=uint32_t  -o ${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix_u8_u32.comp.spv
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix.comp
    COMMENT "Compiling SPIR-V: ${SHADER_SRC_NAME}"
    VERBATIM
    COMMAND_EXPAND_LISTS
)

add_custom_target(cooperative_matrix_spv ALL DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix_f16_f32.comp.spv
	${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix_f16_f16.comp.spv
	${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix_i8_i32.comp.spv
	${CMAKE_CURRENT_SOURCE_DIR}/cooperative_matrix_u8_u32.comp.spv
)
add_dependencies(${EXAMPLE_NAME} cooperative_matrix_spv)

else() # Vulkan_glslc_FOUND
message( WARNING "coompative_matrix example requires glslc")
endif() # Vulkan_glslc_FOUND

else() # Vulkan >= "1.3.300"
message( WARNING "coompative_matrix example requires Vk_NV_cooperative_matrix2, which is only available in Vulkan >= 1.3.300")
endif() # Vulkan >= "1.3.300"