#pragma once

#include "vk_types.hpp"

namespace V {
  
  static std::expected<uint32_t, std::string> findMemType(uint32_t typeFilter, vk::MemoryPropertyFlags props, vk::raii::PhysicalDevice& pDev) {
    
    vk::PhysicalDeviceMemoryProperties memProps = pDev.getMemoryProperties();
    for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
      if((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
        return i;
      }
    }
    
    return std::unexpected("no suitable memory type");
    
  }
  
  static bool createBuf(
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags memProps,
    vk::raii::Buffer& buf,
    vk::raii::DeviceMemory& mem,
    vk::raii::PhysicalDevice& pDev,
    vk::raii::Device& lDev
  ) {
    vk::BufferCreateInfo info{
      .size = size,
      .usage = usage,
      .sharingMode = vk::SharingMode::eExclusive
    };
    
    {
      auto res = lDev.createBuffer(info);
      if(!res) {
        Logger::error("Failed to create staging buffer: {}", vk::to_string(res.error()));
        return false;
      }
      buf = std::move(res.value());
    }
    
    vk::MemoryRequirements memReq = buf.getMemoryRequirements();
    uint32_t typeStaging = 0;
    {
      auto res = findMemType(memReq.memoryTypeBits, memProps, pDev);
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
      auto res = lDev.allocateMemory(allocInfo);
      if(!res) {
        Logger::error("Failed to allocate buffer memory: {}", vk::to_string(res.error()));
        return false;
      }
      mem = std::move(res.value());
    }
    
    buf.bindMemory(*mem, 0);
    return true;
  }
  
  static bool beginSingleTimeComs(vk::raii::CommandBuffer& buf, vk::raii::Device& lDev, vk::raii::CommandPool& cmdPool) {
    
    vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = cmdPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1
    };
    
    {
      auto res = lDev.allocateCommandBuffers(allocInfo);
      if(!res) {
        Logger::error("Failed to allocate command buffer: {}", vk::to_string(res.error()));
        return false;
      }
      buf = std::move(res.value().front());
    }
    
    buf.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    
    return true;
  }
  
  static bool endSingleTimeComs(vk::raii::CommandBuffer& buf, vk::raii::Queue& graphQ) {
    
    buf.end();
    
    graphQ.submit(vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &*buf}, nullptr);
    graphQ.waitIdle();
    
    return true;
  }
  
  static bool copyBuffer(
    vk::raii::Buffer& srcBuf,
    vk::raii::Buffer& dstBuf,
    vk::DeviceSize size,
    vk::raii::Device& lDev,
    vk::raii::CommandPool& cmdPool,
    vk::raii::Queue& graphQ
  ) {
    
    vk::raii::CommandBuffer comCopyBuf{nullptr};
    if(!beginSingleTimeComs(comCopyBuf, lDev, cmdPool)) return false;
    
    comCopyBuf.copyBuffer(srcBuf, dstBuf, vk::BufferCopy(0, 0, size));
    
    if(!endSingleTimeComs(comCopyBuf, graphQ)) return false;
    
    return true;
  }
  
}; //V