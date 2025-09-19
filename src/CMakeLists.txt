add_subdirectory(core)
# add_subdirectory(renderer/vulkan)
add_subdirectory(renderer)
# add_subdirectory(graphics)

add_executable(${PROJECT_NAME}
  main.cpp
)

add_dependencies(${PROJECT_NAME} compile_shaders)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
  VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
  VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  Core
  # Graphics
  
  # assimp::assimp
  glfw
  fmt::fmt
  Vulkan::Vulkan
  # opengl32
  # glad::glad
  # imgui::imgui
  # $ENV{VULKAN_SDK}/Lib/shaderc_combined.lib
)

# file(GLOB SHADER_FILES ${CMAKE_SOURCE_DIR}/shaders/*.vert ${CMAKE_SOURCE_DIR}/shaders/*.frag)
# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  # COMMENT "Copying shaders to build directory..."
  # COMMAND ${CMAKE_COMMAND} -E copy_directory
          # ${CMAKE_SOURCE_DIR}/shaders
          # $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
  # COMMAND ${CMAKE_COMMAND} -E copy_directory
          # ${CMAKE_SOURCE_DIR}/res
          # $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
  # COMMENT "End copying shaders"
# )