#pragma once

#include "vk_types.hpp"

namespace V {
  
  struct VulkanPplConfig {
    
    std::string_view shaderPath;
    
    vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
    
    vk::PolygonMode polygonMode = vk::PolygonMode::eFill;
    vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;
    vk::FrontFace frontface = vk::FrontFace::eCounterClockwise;
    
    bool alphaBlend = false;
    
    bool depthTestEnable = true;
    bool depthWriteEnable = true;
    vk::CompareOp depthCompOp = vk::CompareOp::eLess;
    
  };
  
  class VulkanSwapchain;
  
  class VulkanPipeline {
  public:
    
    VulkanPipeline();
    ~VulkanPipeline();
    
    bool init(const vk::raii::Device&, VulkanSwapchain&, vk::raii::DescriptorSetLayout&, vk::Format, const VulkanPplConfig&);
    
    vk::raii::Pipeline& getPipeline() { return m_pipeline; }
    vk::raii::PipelineLayout& getPipLayout() { return m_pipelineLayout; }
    
  private:
    
    static std::optional<std::vector<char>> readFile(const std::string_view filename);
    [[nodiscard]] std::optional<vk::raii::ShaderModule> createShaderModule(const vk::raii::Device& dev, const std::vector<char>& code) const;
    
    vk::raii::Pipeline m_pipeline{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    
  };
  
}; //V