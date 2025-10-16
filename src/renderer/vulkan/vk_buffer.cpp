#include "vk_buffer.hpp"

namespace V {
  
  VulkanBuffer::VulkanBuffer() {
    
  }
  
  VulkanBuffer::~VulkanBuffer() {
    
  }
  
  bool VulkanBuffer::init(
    vk::raii::PhysicalDevice& pDev,
    vk::raii::Device& lDev,
    VulkanSwapchain& sc,
    vk::raii::CommandPool& cmdP,
    vk::raii::Queue& gQ,
    size_t& cFr
  ) {
    
    m_physDev = &pDev;
    m_logDev = &lDev;
    m_sc = &sc;
    m_cmdPool = &cmdP;
    m_graphQ = &gQ;
    m_curFrame = &cFr;
    
    return true;
  }
  
  
  std::expected<uint32_t, std::string> VulkanBuffer::findMemType(uint32_t typeFilter, vk::MemoryPropertyFlags props) {
    
    vk::PhysicalDeviceMemoryProperties memProps = m_physDev->getMemoryProperties();
    for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
      if((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
        return i;
      }
    }
    
    return std::unexpected("no suitable memory type");
    
  }
  
  bool VulkanBuffer::createBuf(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memProps, vk::raii::Buffer& buf, vk::raii::DeviceMemory& mem) {
    vk::BufferCreateInfo info{
      .size = size,
      .usage = usage,
      .sharingMode = vk::SharingMode::eExclusive
    };
    
    {
      auto res = m_logDev->createBuffer(info);
      if(!res) {
        Logger::error("Failed to create staging buffer: {}", vk::to_string(res.error()));
        return false;
      }
      buf = std::move(res.value());
    }
    
    vk::MemoryRequirements memReq = buf.getMemoryRequirements();
    uint32_t typeStaging = 0;
    {
      auto res = findMemType(memReq.memoryTypeBits, memProps);
      if(!res) {
        Logger::error("Failed to find suitable memory type");
        return false;
      }
      typeStaging = res.value();
    }
    
    vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memReq.size,
      .memoryTypeIndex = typeStaging
    };
    
    {
      auto res = m_logDev->allocateMemory(allocInfo);
      if(!res) {
        Logger::error("Failed to allocate buffer memory: {}", vk::to_string(res.error()));
        return false;
      }
      mem = std::move(res.value());
    }
    
    buf.bindMemory(*mem, 0);
    return true;
  }
  
  bool VulkanBuffer::beginSingleTimeComs(vk::raii::CommandBuffer& buf) {
    
    vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = *m_cmdPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1
    };
    
    {
      auto res = m_logDev->allocateCommandBuffers(allocInfo);
      if(!res) {
        Logger::error("Failed to allocate command buffer: {}", vk::to_string(res.error()));
        return false;
      }
      buf = std::move(res.value().front());
    }
    
    buf.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    
    return true;
  }
  
  bool VulkanBuffer::endSingleTimeComs(vk::raii::CommandBuffer& buf) {
    
    buf.end();
    
    m_graphQ->submit(vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &*buf}, nullptr);
    m_graphQ->waitIdle();
    
    return true;
  }
  
  bool VulkanBuffer::copyBuffer(vk::raii::Buffer& srcBuf, vk::raii::Buffer& dstBuf, vk::DeviceSize size) {
    
    vk::raii::CommandBuffer comCopyBuf{nullptr};
    if(!beginSingleTimeComs(comCopyBuf)) return false;
    
    comCopyBuf.copyBuffer(srcBuf, dstBuf, vk::BufferCopy(0, 0, size));
    
    if(!endSingleTimeComs(comCopyBuf)) return false;
    
    return true;
  }
  
  void VulkanBuffer::updUBO() {
    static auto startTime = std::chrono::high_resolution_clock::now();
    
    auto curTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(curTime - startTime).count();
    
    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.f), time * glm::radians(30.f), glm::vec3(0.f, 0.f, 1.f));
    ubo.view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
    ubo.proj = glm::perspective(glm::radians(45.f), static_cast<float>(m_sc->getExtent().width) / static_cast<float>(m_sc->getExtent().height), 0.1f, 10.f);
    
    ubo.proj[1][1] *= -1;
    
    memcpy(m_uniformBufsMapped[*m_curFrame], &ubo, sizeof(ubo));
  }
  
  
  bool VulkanBuffer::createVBuf() {
    
    vk::DeviceSize bufSize = sizeof(vertices[0]) * vertices.size();
    vk::raii::Buffer stagingBuf{nullptr};
    vk::raii::DeviceMemory stagingBufMem{nullptr};
    if(!createBuf(
      bufSize,
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      stagingBuf,
      stagingBufMem
    )) return false;
    
    void* dataStaging = stagingBufMem.mapMemory(0, bufSize);
    memcpy(dataStaging, vertices.data(), bufSize);
    stagingBufMem.unmapMemory();
    
    if(!createBuf(
      bufSize,
      vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      m_vertBuf,
      m_vertBufMem
    )) return false;
    
    copyBuffer(stagingBuf, m_vertBuf, bufSize);
    
    return true;
  }
  
  bool VulkanBuffer::createIBuf() {
    vk::DeviceSize bufSize = sizeof(indices[0]) * indices.size();
    
    vk::raii::Buffer stagingBuf{nullptr};
    vk::raii::DeviceMemory stagingBufMem{nullptr};
    if(!createBuf(
      bufSize,
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      stagingBuf,
      stagingBufMem
    )) return false;
    
    void* data = stagingBufMem.mapMemory(0, bufSize);
    memcpy(data, indices.data(), bufSize);
    stagingBufMem.unmapMemory();
    
    if(!createBuf(
      bufSize,
      vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      m_indBuf,
      m_indBufMem
    )) return false;
    
    copyBuffer(stagingBuf, m_indBuf, bufSize);
    
    return true;
  }
  
  bool VulkanBuffer::createUBufs() {
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
        bufMem
      )) return false;
      m_uniformBufs.emplace_back(std::move(buf));
      m_uniformBufsMem.emplace_back(std::move(bufMem));
      m_uniformBufsMapped.emplace_back(m_uniformBufsMem[i].mapMemory(0, bufSize));
    }
    
    return true;
  }
  
  
  
  
  
}; //V