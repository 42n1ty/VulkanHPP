#pragma once

#include "vk_types.hpp"

namespace V {
  
  const int MAX_BONES_PER_VERTEX = 4;
  
  struct Vertex {
    glm::vec3 pos;
    glm::vec3 clr;
    glm::vec2 texCoord;
    
    glm::ivec4 boneIDs {-1, -1, -1, -1};
    glm::vec4 weights {0.f, 0.f, 0.f, 0.f};
    
    static vk::VertexInputBindingDescription getBindingDescription() {
      return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }
    
    static std::array<vk::VertexInputAttributeDescription, 5> getAttribDescription() {
      return {
        vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
        vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, clr)),
        vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)),
        vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32G32B32A32Sint, offsetof(Vertex, boneIDs)),
        vk::VertexInputAttributeDescription(4, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, weights)),
      };
    }
  };
  
}; //V