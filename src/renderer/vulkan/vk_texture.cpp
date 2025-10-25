#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "vk_texture.hpp"

namespace V {
  
  VulkanTexture::VulkanTexture() {
    
  }
  VulkanTexture::~VulkanTexture() {
    
  }
  
  bool VulkanTexture::init(
    std::string& path,
    vk::raii::PhysicalDevice& pDev,
    vk::raii::Device& lDev,
    vk::raii::CommandPool& cmdPool,
    vk::raii::Queue& graphQ
  ) {
    if(  !createTextureImg(path, pDev, lDev, cmdPool, graphQ)
      || !createTextureImgView(lDev)
      || !createTextureSampler(pDev, lDev)
    ) return false;
    
    s_path = path;
    return true;
  }
  
  
  bool VulkanTexture::createTextureImg(
    std::string& path,
    vk::raii::PhysicalDevice& pDev,
    vk::raii::Device& lDev,
    vk::raii::CommandPool& cmdPool,
    vk::raii::Queue& graphQ
  ) {
    
    int texWidth, texHeight, texChannels;
    // Logger::debug("{}", path.data());
    stbi_uc* pixels = stbi_load(path.data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imgSize = texWidth * texHeight * 4;
    
    if(!pixels) {
      Logger::error("Failed to load texture image");
      return false;
    }
    
    vk::raii::Buffer stagingBuf({nullptr});
    vk::raii::DeviceMemory stagingBufMem({nullptr});
    
    if(!createBuf(
      imgSize,
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      stagingBuf,
      stagingBufMem,
      pDev,
      lDev
    )) return false;
    void* data = stagingBufMem.mapMemory(0, imgSize);
    memcpy(data, pixels, imgSize);
    stagingBufMem.unmapMemory();
    stbi_image_free(pixels);
    
    if(!createImage(
      texWidth,
      texHeight,
      vk::Format::eR8G8B8A8Srgb,
      vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      m_texImg,
      m_texImgMem,
      pDev,
      lDev
    )) return false;
    
    if(!transitionImageLayout(lDev, cmdPool, graphQ, m_texImg, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal)) return false;
    if(!copyBufToImg(stagingBuf, m_texImg, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), lDev, cmdPool, graphQ)) return false;
    if(!transitionImageLayout(lDev, cmdPool, graphQ, m_texImg, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal)) return false;
    
    return true;
  }
  
  bool VulkanTexture::createTextureImgView(vk::raii::Device& lDev) {
    
    if(!createImgView(
      *m_texImg,
      vk::Format::eR8G8B8A8Srgb,
      vk::ImageAspectFlagBits::eColor,
      m_texImgView,
      lDev
    )) {
      return false;
    }
    
    return true;
  }
  
  bool VulkanTexture::createTextureSampler(
    vk::raii::PhysicalDevice& pDev,
    vk::raii::Device& lDev
  ) {
    
    vk::PhysicalDeviceProperties props = pDev.getProperties();
    
    vk::SamplerCreateInfo samplerInfo{
      .flags = {},
      .magFilter = vk::Filter::eLinear,
      .minFilter = vk::Filter::eLinear,
      .mipmapMode = vk::SamplerMipmapMode::eLinear,
      .addressModeU = vk::SamplerAddressMode::eRepeat,
      .addressModeV = vk::SamplerAddressMode::eRepeat,
      .addressModeW = vk::SamplerAddressMode::eRepeat,
      .mipLodBias = 0.f,
      .anisotropyEnable = 1,
      .maxAnisotropy = props.limits.maxSamplerAnisotropy,
      .compareEnable = vk::False,
      .compareOp = vk::CompareOp::eAlways,
      .minLod = 0.f,
      .maxLod = 0.f,
      .borderColor = vk::BorderColor::eIntOpaqueBlack,
      .unnormalizedCoordinates = vk::False
    };
    
    auto res = lDev.createSampler(samplerInfo);
    if(!res) {
      Logger::error("Failed to create texture sampler: {}", vk::to_string(res.error()));
      return false;
    }
    m_texSampler = std::move(res.value());
    
    return true;
  }

}; //V