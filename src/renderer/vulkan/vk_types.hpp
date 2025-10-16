#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#define VULKAN_HPP_NO_EXCEPTIONS 1

#define NOMINMAX
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <chrono>
#include <thread>
#include <ranges>
#include <limits>
#include <fstream>
#include <set>



#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
// #include <vulkan/vk_enum_string_helper.h>
#include <vma/vk_mem_alloc.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "../../tools/logger/logger.hpp"

namespace V {
  
  const uint32_t MAX_FRAMES_IN_FLIGHT = 2;
  
}; //V