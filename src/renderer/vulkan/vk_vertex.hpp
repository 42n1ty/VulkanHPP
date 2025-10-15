#pragma once

#include "vk_types.hpp"

namespace V {
  
  struct Vertex {
    glm::vec3 pos;
    glm::vec3 clr;
    glm::vec2 texCoord;
    
    static vk::VertexInputBindingDescription getBindingDescription() {
      return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }
    
    static std::array<vk::VertexInputAttributeDescription, 3> getAttribDescription() {
      return {
        vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
        vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, clr)),
        vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord))
      };
    }
  };
  
  const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.f}, {1.f, 0.f, 0.f}, {1.f, 0.f}},
    {{0.5f, -0.5f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
    {{0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 1.f}},
    {{-0.5f, 0.5f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
    
    {{-0.5f, -0.5f, -0.5f}, {1.f, 0.f, 0.f}, {1.f, 0.f}},
    {{0.5f, -0.5f, -0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
    {{0.5f, 0.5f, -0.5f}, {0.f, 0.f, 1.f}, {0.f, 1.f}},
    {{-0.5f, 0.5f, -0.5f}, {1.f, 1.f, 1.f}, {1.f, 1.f}}
  };
  
  const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
  };
  
}; //V