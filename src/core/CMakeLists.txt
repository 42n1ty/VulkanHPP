set(MODULE Core)

add_library(${MODULE} STATIC
	app.cpp
	window.cpp
  # vulkan_base.cpp
  # ui/ui_layer.cpp
)

target_include_directories(${MODULE} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${MODULE} PUBLIC
	fmt::fmt
  VulkanRenderer
	glfw
  Vulkan::Vulkan
	# glad::glad
  # imgui::imgui
)