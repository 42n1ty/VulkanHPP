#pragma once

#include "vk_pipeline.hpp"
#include "vk_texture.hpp"



namespace V {
  
  class VulkanSwapchain;
  
  class VulkanMaterial {
  public:
    
    VulkanMaterial();
    ~VulkanMaterial();
    
    bool init(
      const VulkanPplConfig& config,
      std::shared_ptr<VulkanTexture> texture,
      vk::raii::Device& lDev,
      VulkanSwapchain& sc,
      vk::raii::DescriptorSetLayout& perFrameLayout, // layout set=0
      vk::raii::DescriptorSetLayout& perMaterialLayout, // layout set=1
      vk::raii::DescriptorPool& descPool,
      vk::Format depthFormat
    );
    
    void bind(vk::raii::CommandBuffer& cmdBuf);
    
    vk::raii::PipelineLayout& getPipLayout() { return m_pipeline.getPipLayout(); }
    
  private:
    
    std::shared_ptr<VulkanTexture> m_texture;
    VulkanPipeline m_pipeline;
    vk::raii::DescriptorSet m_descSet{nullptr};
    
  };
  
}; //V