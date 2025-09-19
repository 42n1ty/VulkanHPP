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

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "../../tools/logger/logger.hpp"

namespace CS {
  
  #define VK_CHECK(x)\
  do {\
    VkResult err = x;\
    if(err) {\
      Logger::error("Detected Vulkan error: {}", string_VkResult(err));\
      abort();\
    }\
  } while(0)\
  
}; //CS