#pragma once

#include "vk_swapchain.hpp"
#include "vk_ubo.hpp"
#include "vk_model.hpp"

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
    
    // INIT FUNCS====================================================================================================
    bool createInstance();
    bool setupDM();
    bool pickPhysDev();
    bool createLogDev();
    bool createSurf(Window& wnd);
    bool createSwapchain(Window& wnd);
    bool createImgViews();
    bool createDescSetLayouts();
    bool createCmdPool();
    
    bool createUBO();
    bool createModel();
    
    bool createDepthRes();
    
    bool createDescPool();
    bool createDescSets();
    bool createCmdBufs();
    bool createSyncObjs();
    // INIT FUNCS====================================================================================================
    
    // HELPERS FUNCS====================================================================================================
    std::vector<const char*> getReqExtensions();
    void printDev();
    void recordCmdBuf(uint32_t index);
    void recreateSC();
    void cleanupSC();
    // HELPERS FUNCS====================================================================================================
    
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
    // vk::raii::DescriptorSetLayout m_descSetLayout{nullptr};
    vk::raii::DescriptorSetLayout m_perFrameDescSetLayout{nullptr};
    vk::raii::DescriptorSetLayout m_perMatDescSetLayout{nullptr};
    vk::raii::DescriptorPool m_descPool{nullptr};
    
    vk::raii::Image m_depthImg{nullptr};
    vk::raii::DeviceMemory m_depthImgMem{nullptr};
    vk::raii::ImageView m_depthImgView{nullptr};
    
    std::vector<vk::raii::CommandBuffer> m_cmdBufs;
    std::vector<vk::raii::Semaphore> m_presCompleteSems;
    std::vector<vk::raii::Semaphore> m_renderFinishedSems;
    std::vector<vk::raii::Fence> m_inFlightFences;
    std::vector<vk::Fence> m_imagesInFlight;
    std::vector<vk::raii::DescriptorSet> m_perFrameDescSets;
    
    std::unique_ptr<VulkanModel> m_model{nullptr};
    
    UBOManager<CameraData> m_cameraUBO;
    UBOManager<ObjectData> m_objectUBO;
    UBOManager<BoneData> m_bonesUBO;
    
    VulkanSwapchain m_sc;
    
    std::vector<vk::raii::ImageView> m_imgViews;
    
    std::chrono::time_point<std::chrono::high_resolution_clock> m_lastFrameTime;
    
    Window* m_wnd;
    
    size_t m_curFrame = 0;
    uint32_t m_graphQI;
    uint32_t m_presQI;
    
    #ifdef NDEBUG
      const bool enableValidLayers = false;
    #else
      const bool enableVaildLayers = true;
    #endif
  };
  
}; //V