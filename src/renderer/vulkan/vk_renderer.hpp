#pragma once

#include "vk_types.hpp"
#include "vk_swapchain.hpp"
#include "vk_pipeline.hpp"

#include <expected>

struct GLFWwindow;

namespace V {
  
  class Window;
  
  
  class VulkanRenderer {
  public:
    VulkanRenderer();
    ~VulkanRenderer();
    
    bool s_isInit{false};
    int s_frameNum{0};
    bool s_stopRendering{false};
    
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    
    bool init(Window& wnd);
    void cleanup();
    
    bool drawFrame();
    void wait() { m_logDev.waitIdle(); }
    static void framebufferResizeCallback(GLFWwindow* wnd, int w, int h);
    
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
      vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
      vk::DebugUtilsMessageTypeFlagsEXT type,
      const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
      void*
    ) {
      if(severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        Logger::error("Validation layer: type {}; msg: {}", vk::to_string(type), pCallbackData->pMessage);
      }
      
      return vk::False;
    }
    
    bool framebufferResized = false;
    
  private:
  
    bool createInstance();
    bool setupDM();
    bool pickPhysDev();
    bool createLogDev();
    bool createSurf(Window& wnd);
    bool createSwapchain(Window& wnd);
    bool createImgViews();
    bool createGraphPipeline();
    bool createCmdPool();
    bool createVBuf();
    bool createCmdBufs();
    bool createSyncObjs();
    
    std::vector<const char*> getReqExtensions();
    void printDev();
    void recordCmdBuf(uint32_t index);
    void transitionImgLayout(
      uint32_t imgInd,
      vk::ImageLayout oldLayout,
      vk::ImageLayout newLayout,
      vk::AccessFlags2 srcAccessMask,
      vk::AccessFlags2 dstAccessMask,
      vk::PipelineStageFlags2 srcStageMask,
      vk::PipelineStageFlags2 dstStageMask
    );
    void recreateSC();
    void cleanupSC();
    std::expected<uint32_t, std::string> findMemType(uint32_t typeFilter, vk::MemoryPropertyFlags props);
    
    const std::vector<const char*> m_validLayers = {
      "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*> m_devExtensions = {
      vk::KHRSwapchainExtensionName,
      vk::KHRSpirv14ExtensionName,
      vk::KHRSynchronization2ExtensionName,
      vk::KHRCreateRenderpass2ExtensionName
    };
    
    vk::raii::Context m_ctx;
    vk::raii::Instance m_inst{nullptr};
    vk::raii::DebugUtilsMessengerEXT m_debMesser{nullptr};
    vk::raii::PhysicalDevice m_physDev{nullptr};
    vk::raii::Device m_logDev{nullptr};
    vk::raii::Queue m_graphQ{nullptr};
    vk::raii::Queue m_presQ{nullptr};
    vk::raii::SurfaceKHR m_surf{nullptr};
    vk::raii::CommandPool m_cmdPool{nullptr};
    vk::raii::Buffer m_vertBuf{nullptr};
    vk::raii::DeviceMemory m_vertBufMem{nullptr};
    
    std::vector<vk::raii::CommandBuffer> m_cmdBufs;
    std::vector<vk::raii::Semaphore> m_presCompleteSems;
    std::vector<vk::raii::Semaphore> m_renderFinishedSems;
    std::vector<vk::raii::Fence> m_inFlightFences;
    std::vector<vk::Fence> m_imagesInFlight;
    
    VulkanSwapchain m_sc;
    VulkanPipeline m_pipeline;
    
    std::vector<vk::raii::ImageView> m_imgViews;
    
    Window* m_wnd;
    
    size_t m_curFrame = 0;
    uint32_t m_graphQI;
    uint32_t m_presQI;
    
    const uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    
    #ifdef NDEBUG
      const bool enableValidLayers = false;
    #else
      const bool enableVaildLayers = true;
    #endif
  };
  
}; //V