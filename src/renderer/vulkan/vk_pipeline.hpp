#pragma once

#include "vk_types.hpp"

namespace V {
  
  class VulkanSwapchain;
  
  class VulkanPipeline {
  public:
    
    VulkanPipeline();
    ~VulkanPipeline();
    
    bool init(const vk::raii::Device&, VulkanSwapchain&);
    
    vk::raii::Pipeline& getPipeline() { return m_pipeline; }
    vk::raii::DescriptorSetLayout& getDescSetLayout() { return m_descSetLayout; }
    vk::raii::PipelineLayout& getPipLayout() { return m_pipelineLayout; }
    
  private:
    
    static std::optional<std::vector<char>> readFile(const std::string_view filename);
    [[nodiscard]] std::optional<vk::raii::ShaderModule> createShaderModule(const vk::raii::Device& dev, const std::vector<char>& code) const;
    
    vk::raii::Pipeline m_pipeline{nullptr};
    vk::raii::DescriptorSetLayout m_descSetLayout{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    
  };
  
}; //V