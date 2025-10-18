#pragma once

#include "vk_buffer.hpp"

namespace V {
  
  struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
  };
  
  class VulkanUBO {
  public:
    
    VulkanUBO() {}
    ~VulkanUBO() {}
    
    bool init(vk::raii::PhysicalDevice& pDev, vk::raii::Device& lDev) {
      if(!createUBufs(pDev, lDev)) return false;
      
      return true;
    }
    
    std::vector<vk::raii::Buffer>& getUBufs() { return m_uniformBufs; }
    
    void updUBO(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj, uint32_t curFrame) {
      
      UniformBufferObject ubo{};
      ubo.model = model;
      ubo.view = view;
      ubo.proj = proj;
      
      ubo.proj[1][1] *= -1;
      
      memcpy(m_uniformBufsMapped[curFrame], &ubo, sizeof(ubo));
    }
    
  private:
    bool createUBufs(vk::raii::PhysicalDevice& pDev, vk::raii::Device& lDev) {
      m_uniformBufs.clear();
      m_uniformBufsMem.clear();
      m_uniformBufsMapped.clear();
      
      for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vk::DeviceSize bufSize = sizeof(UniformBufferObject);
        vk::raii::Buffer buf{nullptr};
        vk::raii::DeviceMemory bufMem{nullptr};
        if(!createBuf(
          bufSize,
          vk::BufferUsageFlagBits::eUniformBuffer,
          vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
          buf,
          bufMem,
          pDev,
          lDev
        )) return false;
        
        m_uniformBufs.emplace_back(std::move(buf));
        m_uniformBufsMem.emplace_back(std::move(bufMem));
        m_uniformBufsMapped.emplace_back(m_uniformBufsMem[i].mapMemory(0, bufSize));
      }
      
      return true;
    }
    
    std::vector<vk::raii::Buffer> m_uniformBufs;
    std::vector<vk::raii::DeviceMemory> m_uniformBufsMem;
    std::vector<void*> m_uniformBufsMapped;
    
  };
  
}; //V