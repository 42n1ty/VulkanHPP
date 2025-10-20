set(MODULE VulkanRenderer)

add_library(${MODULE} STATIC
  vk_renderer.cpp
  vk_swapchain.cpp
  vk_pipeline.cpp
  vk_texture.cpp
)

target_include_directories(${MODULE} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
  ${Vulkan_INCLUDE_DIRS}
  ${Stb_INCLUDE_DIR}
)

target_link_libraries(${MODULE} PUBLIC 
  
)

target_link_libraries(${MODULE} PRIVATE
	fmt::fmt
  
  Vulkan::Vulkan
  # unofficial::shaderc::shaderc
)