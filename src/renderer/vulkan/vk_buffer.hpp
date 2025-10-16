#pragma once

#include "vk_types.hpp"
#include "vk_vertex.hpp"
#include "vk_ubo.hpp"
#include "vk_swapchain.hpp"

namespace V {
  
  class VulkanBuffer {
  public:
    VulkanBuffer();
    ~VulkanBuffer();
    
    bool init(
      vk::raii::PhysicalDevice& pDev,
      vk::raii::Device& lDev,
      VulkanSwapchain& sc,
      vk::raii::CommandPool& cmdP,
      vk::raii::Queue& gQ,
      size_t& cFr
    );
    
    bool createVBuf();
    bool createIBuf();
    bool createUBufs();
    
    vk::raii::Buffer& getVBuf() { return m_vertBuf; }
    vk::raii::Buffer& getIBuf() { return m_indBuf; }
    std::vector<vk::raii::Buffer>& getUBufs() { return m_uniformBufs; }
    std::expected<uint32_t, std::string> findMemType(uint32_t typeFilter, vk::MemoryPropertyFlags props);
    bool createBuf(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memProps, vk::raii::Buffer& buf, vk::raii::DeviceMemory& mem);
    bool beginSingleTimeComs(vk::raii::CommandBuffer& buf);
    bool endSingleTimeComs(vk::raii::CommandBuffer& buf);
    void updUBO();
    
  private:
    
    bool copyBuffer(vk::raii::Buffer& srcBuf, vk::raii::Buffer& dstBuf, vk::DeviceSize size);
    
    vk::raii::Buffer m_vertBuf{nullptr};
    vk::raii::DeviceMemory m_vertBufMem{nullptr};
    vk::raii::Buffer m_indBuf{nullptr};
    vk::raii::DeviceMemory m_indBufMem{nullptr};
    std::vector<vk::raii::Buffer> m_uniformBufs;
    std::vector<vk::raii::DeviceMemory> m_uniformBufsMem;
    std::vector<void*> m_uniformBufsMapped;
    
    vk::raii::PhysicalDevice* m_physDev{nullptr};
    vk::raii::Device* m_logDev{nullptr};
    VulkanSwapchain* m_sc;
    vk::raii::CommandPool* m_cmdPool{nullptr};
    vk::raii::Queue* m_graphQ{nullptr};
    size_t* m_curFrame{nullptr};
    
  };
  
}; //V