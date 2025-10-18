#pragma once

#include "vk_buffer.hpp"
#include "vk_vertex.hpp"

namespace V {
  
  class VulkanMesh {
  public:
    
    VulkanMesh() {}
    ~VulkanMesh() {}
    
    bool init(
      const std::vector<Vertex>& verts,
      const std::vector<uint32_t>& inds,
      vk::raii::PhysicalDevice& pDev,
      vk::raii::Device& lDev,
      vk::raii::CommandPool& cmdPool,
      vk::raii::Queue& graphQ
    ) {
      if(!createVBuf(
          verts,
          pDev,
          lDev,
          cmdPool,
          graphQ
        )
        ||
        !createIBuf(
          inds,
          pDev,
          lDev,
          cmdPool,
          graphQ
        )
      ) return false;
      
      m_indCnt = inds.size();
      return true;
    }
    
    void bind(vk::raii::CommandBuffer& cmdBuf) {
      cmdBuf.bindVertexBuffers(0, *m_vertBuf, {0});
      cmdBuf.bindIndexBuffer(*m_indBuf, 0, vk::IndexType::eUint32);
    }
    
    uint32_t getIndexCount() { return m_indCnt; }
    
  private:
    
    bool createVBuf(
      const std::vector<Vertex>& verts,
      vk::raii::PhysicalDevice& pDev,
      vk::raii::Device& lDev,
      vk::raii::CommandPool& cmdPool,
      vk::raii::Queue& graphQ
    ) {
      
      vk::DeviceSize bufSize = sizeof(verts[0]) * verts.size();
      vk::raii::Buffer stagingBuf{nullptr};
      vk::raii::DeviceMemory stagingBufMem{nullptr};
      if(!createBuf(
        bufSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuf,
        stagingBufMem,
        pDev,
        lDev
      )) return false;
      
      void* dataStaging = stagingBufMem.mapMemory(0, bufSize);
      memcpy(dataStaging, verts.data(), bufSize);
      stagingBufMem.unmapMemory();
      
      if(!createBuf(
        bufSize,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_vertBuf,
        m_vertBufMem,
        pDev,
        lDev
      )) return false;
      
      copyBuffer(stagingBuf, m_vertBuf, bufSize, lDev, cmdPool, graphQ);
      
      return true;
    }
  
    bool createIBuf(
      const std::vector<uint32_t>& inds,
      vk::raii::PhysicalDevice& pDev,
      vk::raii::Device& lDev,
      vk::raii::CommandPool& cmdPool,
      vk::raii::Queue& graphQ
    ) {
      vk::DeviceSize bufSize = sizeof(inds[0]) * inds.size();
      vk::raii::Buffer stagingBuf{nullptr};
      vk::raii::DeviceMemory stagingBufMem{nullptr};
      if(!createBuf(
        bufSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuf,
        stagingBufMem,
        pDev,
        lDev
      )) return false;
      
      void* data = stagingBufMem.mapMemory(0, bufSize);
      memcpy(data, inds.data(), bufSize);
      stagingBufMem.unmapMemory();
      
      if(!createBuf(
        bufSize,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_indBuf,
        m_indBufMem,
        pDev,
        lDev
      )) return false;
      
      copyBuffer(stagingBuf, m_indBuf, bufSize, lDev, cmdPool, graphQ);
      
      return true;
    }
  
    
    vk::raii::Buffer m_vertBuf{nullptr};
    vk::raii::DeviceMemory m_vertBufMem{nullptr};
    vk::raii::Buffer m_indBuf{nullptr};
    vk::raii::DeviceMemory m_indBufMem{nullptr};
    uint32_t m_indCnt{0};
    
  };
  
}; //V