#pragma once

#include "vk_buffer.hpp"

namespace V {
  
  static bool createImage(
    uint32_t w,
    uint32_t h,
    vk::Format format,
    vk::ImageTiling tiling,
    vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags props,
    vk::raii::Image& image,
    vk::raii::DeviceMemory& imageMem,
    vk::raii::PhysicalDevice& pDev,
    vk::raii::Device& lDev
  ) {
    
    vk::ImageCreateInfo imgInfo{
      .imageType = vk::ImageType::e2D,
      .format = format,
      .extent = vk::Extent3D{w, h, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = vk::SampleCountFlagBits::e1,
      .tiling = tiling,
      .usage = usage,
      .sharingMode = vk::SharingMode::eExclusive,
      .initialLayout = vk::ImageLayout::eUndefined
    };
    
    {
      auto res = lDev.createImage(imgInfo);
      if(!res) {
        Logger::error("Failed to create image: {}", vk::to_string(res.error()));
        return false;
      }
      image = std::move(res.value());
    }
    
    vk::MemoryRequirements memReq = image.getMemoryRequirements();
    uint32_t typeIndex = 0;
    {
      auto res = findMemType(memReq.memoryTypeBits, props, pDev);
      if(!res) {
        Logger::error("Failed to find suitable memory type");
        return false;
      }
      typeIndex = res.value();
    }
    vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memReq.size,
      .memoryTypeIndex = typeIndex
    };
    
    {
      auto res = lDev.allocateMemory(allocInfo);
      if(!res) {
        Logger::error("Failed to allocate image memory: {}", vk::to_string(res.error()));
        return false;
      }
      imageMem = std::move(res.value());
    }
    image.bindMemory(imageMem, 0);
    
    return true;
  }

  static bool transitionImageLayout(
    vk::raii::Device& lDev,
    vk::raii::CommandPool& cmdPool,
    vk::raii::Queue& graphQ,
    const vk::Image& image,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout,
    vk::AccessFlags2 srcAccessMask = {},
    vk::AccessFlags2 dstAccessMask = {},
    vk::PipelineStageFlags2 srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
    vk::PipelineStageFlags2 dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::raii::CommandBuffer* cmdBuf = nullptr
  ) {
    
    vk::raii::CommandBuffer tempCmdBuf{nullptr};
    bool useTemp = (cmdBuf == nullptr);
    
    if(useTemp) {
      if(!beginSingleTimeComs(tempCmdBuf, lDev, cmdPool)) return false;
      cmdBuf = &tempCmdBuf;
    }

    vk::ImageMemoryBarrier2 barrier{
      .srcStageMask = srcStageMask,
      .srcAccessMask = srcAccessMask,
      .dstStageMask = dstStageMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
      }
    };

    if(oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
      barrier.srcAccessMask = {};
      barrier.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
      barrier.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
      barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    } 
    else if(oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
      barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
      barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
      barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
      barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
    }
    else if(oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
      barrier.srcAccessMask = srcAccessMask;
      barrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
      barrier.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
      barrier.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    }
    else if(oldLayout == vk::ImageLayout::eColorAttachmentOptimal && newLayout == vk::ImageLayout::ePresentSrcKHR) {
      barrier.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
      barrier.dstAccessMask = {};
      barrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
      barrier.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe;
    }
    else {
      Logger::error("Unsupported layout transition from {} to {}", vk::to_string(oldLayout), vk::to_string(newLayout));
      return false;
    }

    vk::DependencyInfo dependInfo{
      .dependencyFlags = {},
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier
    };

    cmdBuf->pipelineBarrier2(dependInfo);

    if(useTemp) {
      if(!endSingleTimeComs(tempCmdBuf, graphQ)) return false;
    }
    
    return true;
  }

  static bool copyBufToImg(
    const vk::raii::Buffer& buf,
    vk::raii::Image& img,
    uint32_t w,
    uint32_t h,
    vk::raii::Device& lDev,
    vk::raii::CommandPool& cmdPool,
    vk::raii::Queue& graphQ
  ) {
    vk::raii::CommandBuffer cmdBuf{nullptr};
    if(!beginSingleTimeComs(cmdBuf, lDev, cmdPool)) return false;
    
    vk::BufferImageCopy region{
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
      .imageOffset = {0, 0, 0},
      .imageExtent = {w, h, 1}
    };
    
    cmdBuf.copyBufferToImage(buf, img, vk::ImageLayout::eTransferDstOptimal, {region});
    
    if(!endSingleTimeComs(cmdBuf, graphQ)) return false;
    
    return true;
  }

  static bool createImgView(
    const vk::Image& img,
    vk::Format format,
    vk::ImageAspectFlags aspectFlags,
    vk::raii::ImageView& iv,
    vk::raii::Device& lDev
  ) {
    
    vk::ImageViewCreateInfo viewInfo{
      .flags = {},
      .image = img,
      .viewType = vk::ImageViewType::e2D,
      .format = format,
      .components = {},
      .subresourceRange = { aspectFlags, 0, 1, 0, 1 }
    };
    
    auto res = lDev.createImageView(viewInfo);
    if(!res) {
      Logger::error("Failed to create image view: {}", vk::to_string(res.error()));
      return false;
    }
    iv = std::move(res.value());
    
    return true;
  }
  
}; //V