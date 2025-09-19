#pragma once

#include "vk_types.hpp"

namespace V {
  
  class Window;
  
  class VulkanSwapchain {
  public:
    
    VulkanSwapchain();
    ~VulkanSwapchain();
    
    bool init(
      Window& wnd,
      vk::raii::PhysicalDevice& physDev,
      vk::raii::Device& logDev,
      vk::raii::SurfaceKHR& surf,
      const uint32_t gID,
      const uint32_t pID
    );
    
    vk::raii::SwapchainKHR& getSC() { return m_sc; }
    std::vector<vk::Image>& getImgs() { return m_images; }
    vk::Format& getFormat() {return m_format; }
    vk::Extent2D& getExtent() { return m_extent; }
    
  private:
    
    vk::Format chooseSSF(const std::vector<vk::SurfaceFormatKHR>& avFormats);
    vk::PresentModeKHR chooseSPM(const std::vector<vk::PresentModeKHR>& avModes);
    vk::Extent2D chooseSE(Window& wnd, const vk::SurfaceCapabilitiesKHR& capabs);
    
    vk::raii::SwapchainKHR m_sc{nullptr};
    
    std::vector<vk::Image> m_images;
    vk::Format m_format = vk::Format::eUndefined;
    vk::Extent2D m_extent;
    
  };
  
}; //V