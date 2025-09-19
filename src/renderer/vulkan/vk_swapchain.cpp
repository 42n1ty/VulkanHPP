#include "vk_swapchain.hpp"
#include "../../core/window.hpp"


namespace V {
  
  VulkanSwapchain::VulkanSwapchain() {
    
  }
  
  VulkanSwapchain::~VulkanSwapchain() {
    
  }
  
  //====================================================================================================
  
  
  vk::Format VulkanSwapchain::chooseSSF(const std::vector<vk::SurfaceFormatKHR>& avFormats) {
    const auto formatIt = std::ranges::find_if(avFormats,
      [](const auto& format) {
        return  format.format == vk::Format::eB8G8R8A8Srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
      }
    );
    
    return formatIt != avFormats.end() ? formatIt->format : avFormats[0].format;
  }
  
  vk::PresentModeKHR VulkanSwapchain::chooseSPM(const std::vector<vk::PresentModeKHR>& avModes) {
    return std::ranges::any_of(avModes,
      [](const vk::PresentModeKHR& value) {
        return value == vk::PresentModeKHR::eMailbox;
      }
    ) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
  }
  
  vk::Extent2D VulkanSwapchain::chooseSE(Window& wnd, const vk::SurfaceCapabilitiesKHR& capabs) {
    if(capabs.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabs.currentExtent;
    }
    
    int w, h;
    glfwGetFramebufferSize(wnd.getWindow(), &w, &h);
    
    return {
      std::clamp<uint32_t>(w, capabs.minImageExtent.width, capabs.maxImageExtent.width),
      std::clamp<uint32_t>(h, capabs.minImageExtent.height, capabs.maxImageExtent.height)
    };
  }
  
  
  //====================================================================================================
  
  bool VulkanSwapchain::init(
    Window& wnd,
    vk::raii::PhysicalDevice& physDev,
    vk::raii::Device& logDev,
    vk::raii::SurfaceKHR& surf,
    const uint32_t gID,
    const uint32_t pID
  ) {
    auto surfCapabs = physDev.getSurfaceCapabilitiesKHR(surf);
    m_format = chooseSSF(physDev.getSurfaceFormatsKHR(surf));
    m_extent = chooseSE(wnd, surfCapabs);
    auto minImgCnt = std::max(3u, surfCapabs.minImageCount);
    minImgCnt = (surfCapabs.maxImageCount > 0 && minImgCnt > surfCapabs.maxImageCount) ? surfCapabs.maxImageCount : minImgCnt;
    
    vk::SwapchainCreateInfoKHR createInfo{
      .surface = surf,
      .minImageCount = minImgCnt,
      .imageFormat = m_format,
      .imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
      .imageExtent = m_extent,
      .imageArrayLayers = 1,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform = surfCapabs.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = chooseSPM(physDev.getSurfacePresentModesKHR(surf)),
      .clipped = true,
      .oldSwapchain = nullptr
    };
    
    uint32_t qfIdcs[] = {gID, pID};
    if(gID != pID) {
      createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = qfIdcs;
    }
    else {
      createInfo.imageSharingMode = vk::SharingMode::eExclusive;
      createInfo.queueFamilyIndexCount = 0;
      createInfo.pQueueFamilyIndices = nullptr;
    }
      
    {
      auto res = logDev.createSwapchainKHR(createInfo);
      if(!res) {
        Logger::error("Failed to create swapchain: {}", vk::to_string(res.error()));
        return false;
      }
      
      m_sc = std::move(res.value());
    }
    
    m_images = m_sc.getImages();
    
    return true;
  }
  
}; //V