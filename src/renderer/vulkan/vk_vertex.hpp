#pragma once

#include "vk_types.hpp"

namespace V {
  
  struct Vertex {
    glm::vec2 pos;
    glm::vec3 clr;
    
    static vk::VertexInputBindingDescription getBindingDescription() {
      return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }
    
    static std::array<vk::VertexInputAttributeDescription, 2> getAttribDescription() {
      return {
        vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos)),
        vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, clr))
      };
    }
  };
  
  const std::vector<Vertex> vertices = {
    {{0.f, -0.5f}, {1.f, 0.f, 0.f}},
    {{0.5f, 0.5f}, {0.f, 1.f, 0.f}},
    {{-0.5f, 0.5f}, {0.f, 0.f, 1.f}}
  };
  
}; //V