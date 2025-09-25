#pragma once

#include "vk_types.hpp"

namespace V {
  
  struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
  };
  
}; //V