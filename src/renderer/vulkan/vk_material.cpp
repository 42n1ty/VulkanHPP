#include "vk_material.hpp"
#include "vk_swapchain.hpp"

namespace V {
  
  VulkanMaterial::VulkanMaterial() {}
  VulkanMaterial::~VulkanMaterial() {}
    
  bool VulkanMaterial::init(
    const VulkanPplConfig& config,
    std::shared_ptr<VulkanTexture> texture,
    vk::raii::Device& lDev,
    VulkanSwapchain& sc,
    vk::raii::DescriptorSetLayout& perFrameLayout, // layout set=0
    vk::raii::DescriptorSetLayout& perMaterialLayout, // layout set=1
    vk::raii::DescriptorPool& descPool,
    vk::Format depthFormat
  ) {
    m_texture = texture;
    
    std::array<vk::DescriptorSetLayout, 2> setLayouts = {perFrameLayout, perMaterialLayout};
    vk::PipelineLayoutCreateInfo plInfo{
      .setLayoutCount = setLayouts.size(),
      .pSetLayouts = setLayouts.data()
    };
    
    if(!m_pipeline.init(lDev, sc, plInfo, depthFormat, config)) {
      Logger::error("Failed to create pipeline for material");
      return false;
    }
    
    vk::DescriptorSetAllocateInfo allocInfo{
      .descriptorPool = descPool,
      .descriptorSetCount = 1,
      .pSetLayouts = &(*perMaterialLayout)
    };
    
    auto res = lDev.allocateDescriptorSets(allocInfo);
    if(!res) {
      Logger::error("Failed to allocate material descriptor set: {}", vk::to_string(res.error()));
      return false;
    }
    m_descSet = std::move(res.value()[0]);
    
    vk::DescriptorImageInfo imgInfo{
      .sampler = m_texture->getSampler(),
      .imageView = m_texture->getImgView(),
      .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };
    
    vk::WriteDescriptorSet descWrite{
      .dstSet = *m_descSet,
      .dstBinding = 0, // binding 0 in set=1
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = vk::DescriptorType::eCombinedImageSampler,
      .pImageInfo = &imgInfo
    };
    
    lDev.updateDescriptorSets({descWrite}, {});
    
    return true;
  }
  
  void VulkanMaterial::bind(vk::raii::CommandBuffer& cmdBuf) {
    cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getPipeline());
    
    cmdBuf.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics,
      m_pipeline.getPipLayout(),
      1, // set=1
      {*m_descSet},
      {}
    );
  }
  
}; //V