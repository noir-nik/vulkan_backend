set(EXAMPLE_NAME vector_addition)

file(GLOB_RECURSE EXAMPLE_SOURCE_FILES "*.cpp")

add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE_FILES})

target_link_libraries(${EXAMPLE_NAME} vulkan_backend::vulkan_backend)

target_include_directories(${EXAMPLE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_command(TARGET ${EXAMPLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/vector_addition.comp
    $<TARGET_FILE_DIR:${EXAMPLE_NAME}>
)