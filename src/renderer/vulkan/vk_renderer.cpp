#include "vk_renderer.hpp"
#include "vk_vertex.hpp"
#include "vk_ubo.hpp"

#include "../../core/window.hpp"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "../../tools/logger/logger.hpp"



namespace V {
  
  VulkanRenderer::VulkanRenderer() {
    
  }
  VulkanRenderer::~VulkanRenderer() {
    cleanup();
  }
  
  //====================================================================================================
  
  std::vector<const char*> VulkanRenderer::getReqExtensions() {
    uint32_t glfwExtCnt = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtCnt);
    
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtCnt);
    if(enableVaildLayers) {
      extensions.emplace_back(vk::EXTDebugUtilsExtensionName);
    }
    
    return extensions;
  }
  
  void VulkanRenderer::printDev() {
    
    vk::PhysicalDeviceProperties properties = m_physDev.getProperties();
    Logger::info("Selected Physical Device: {}", properties.deviceName.data());
    Logger::info("  - API Version: {}.{}.{}", 
                VK_API_VERSION_MAJOR(properties.apiVersion), 
                VK_API_VERSION_MINOR(properties.apiVersion), 
                VK_API_VERSION_PATCH(properties.apiVersion));
    Logger::info("  - Type: {}", vk::to_string(properties.deviceType)); 
    Logger::info("  - Vendor ID: {:#x}", properties.vendorID);
    Logger::info("  - Device ID: {:#x}", properties.deviceID);
    Logger::info("  - Max Texture2D Size: {}x{}", 
        properties.limits.maxImageDimension2D, 
        properties.limits.maxImageDimension2D);
    
    vk::PhysicalDeviceMemoryProperties memProperties = m_physDev.getMemoryProperties();
    Logger::info("Memory Heaps ({} total):", memProperties.memoryHeapCount);
    for (uint32_t i = 0; i < memProperties.memoryHeapCount; ++i) {
      const auto& heap = memProperties.memoryHeaps[i];
      float sizeInMB = static_cast<float>(heap.size) / (1024.0f * 1024.0f);
      std::string flagsStr;
      if (heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal) {
        flagsStr = " (Device Local - VRAM)";
      }

      Logger::info("  - Heap {}: {:.2f} MB{}", i, sizeInMB, flagsStr);
    }
  }
  
  void VulkanRenderer::recordCmdBuf(uint32_t index) {
    m_cmdBufs[m_curFrame].begin({});
    
    transitionImgLayout(
      index,
      vk::ImageLayout::eUndefined,
      vk::ImageLayout::eColorAttachmentOptimal,
      {},
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::PipelineStageFlagBits2::eTopOfPipe,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput
    );
    vk::ClearValue clearClr = vk::ClearColorValue(0.f, 0.f, 0.f, 1.f);
    vk::RenderingAttachmentInfo attachmentInfo{
      .imageView = m_imgViews[index],
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clearClr
    };
    vk::RenderingInfo renderingInfo{
      .renderArea = {.offset = {0, 0}, .extent = m_sc.getExtent()},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &attachmentInfo
    };
    
    m_cmdBufs[m_curFrame].beginRendering(renderingInfo);
    m_cmdBufs[m_curFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getPipeline());
    
    m_cmdBufs[m_curFrame].setViewport(0, vk::Viewport(0.f, 0.f, static_cast<float>(m_sc.getExtent().width), static_cast<float>(m_sc.getExtent().height), 0.f, 1.f));
    m_cmdBufs[m_curFrame].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), m_sc.getExtent()));
    
    m_cmdBufs[m_curFrame].bindVertexBuffers(0, *m_vertBuf, {0});
    m_cmdBufs[m_curFrame].bindIndexBuffer(*m_indBuf, 0, vk::IndexType::eUint16);
    m_cmdBufs[m_curFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline.getPipLayout(), 0, *(m_descSets[m_curFrame]), nullptr);
    m_cmdBufs[m_curFrame].drawIndexed(indices.size(), 1, 0, 0, 0);
    
    m_cmdBufs[m_curFrame].endRendering();
    
    transitionImgLayout(
      index,
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageLayout::ePresentSrcKHR,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      {},
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eBottomOfPipe
    );
    
    m_cmdBufs[m_curFrame].end();
  }
  
  void VulkanRenderer::transitionImgLayout(
    uint32_t imgInd,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout,
    vk::AccessFlags2 srcAccessMask,
    vk::AccessFlags2 dstAccessMask,
    vk::PipelineStageFlags2 srcStageMask,
    vk::PipelineStageFlags2 dstStageMask
  ) {
    vk::ImageMemoryBarrier2 barrier{
      .srcStageMask = srcStageMask,
      .srcAccessMask = srcAccessMask,
      .dstStageMask = dstStageMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = m_sc.getImgs()[imgInd],
      .subresourceRange = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
      }
    };
    
    vk::DependencyInfo dependInfo{
      .dependencyFlags = {},
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier
    };
    
    m_cmdBufs[m_curFrame].pipelineBarrier2(dependInfo);
  }
  
  bool VulkanRenderer::drawFrame() {
    
    if (framebufferResized) {
      framebufferResized = false;
      recreateSC();
      return true;
    }
    
    auto fenceRes = m_logDev.waitForFences(*m_inFlightFences[m_curFrame], vk::True, UINT64_MAX);
    if (fenceRes != vk::Result::eSuccess) {
      Logger::error("failed to wait for fence!");
      return false;
    }
    uint32_t imgIndex = 0;
    auto res = m_sc.getSC().acquireNextImage(UINT64_MAX, *m_presCompleteSems[m_curFrame], nullptr);
    if(res.first == vk::Result::eErrorOutOfDateKHR) {
      Logger::debug("Window's size has been changed: recreating swapchain");
      framebufferResized = true;
      return true;
    }
    if(res.first != vk::Result::eSuccess && res.first != vk::Result::eSuboptimalKHR) {
      Logger::debug("failed to acqure swapchain image!");
      return false;
    }
    imgIndex = res.second;
    
    if (m_imagesInFlight[imgIndex] != nullptr) {
      (void)m_logDev.waitForFences({m_imagesInFlight[imgIndex]}, vk::True, UINT64_MAX);
    }
    m_imagesInFlight[imgIndex] = *m_inFlightFences[m_curFrame];
    
    updUBO();
    
    m_logDev.resetFences(*m_inFlightFences[m_curFrame]);
    m_cmdBufs[m_curFrame].reset();
    recordCmdBuf(imgIndex);
    
    vk::PipelineStageFlags waitDestStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    const vk::SubmitInfo submitInfo{
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*m_presCompleteSems[m_curFrame],
      .pWaitDstStageMask = &waitDestStageMask,
      .commandBufferCount = 1,
      .pCommandBuffers = &*m_cmdBufs[m_curFrame],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*m_renderFinishedSems[imgIndex]
    };
    m_graphQ.submit(submitInfo, *m_inFlightFences[m_curFrame]);
    
    const vk::PresentInfoKHR presInfo{
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*m_renderFinishedSems[imgIndex],
      .swapchainCount = 1,
      .pSwapchains = &*m_sc.getSC(),
      .pImageIndices = &imgIndex
    };
    res.first = m_presQ.presentKHR(presInfo);
    if(res.first == vk::Result::eErrorOutOfDateKHR || res.first == vk::Result::eSuboptimalKHR) {
      Logger::debug("Window's size was changed: recreating swapchain");
      framebufferResized = true;
    }
    else if(res.first != vk::Result::eSuccess) {
      Logger::error("failed to present swapchain image!");
      return false;
    }
    
    m_curFrame = (m_curFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    
    return true;
    
  }
  
  void VulkanRenderer::recreateSC() {
    
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_wnd->getWindow(), &width, &height);
    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(m_wnd->getWindow(), &width, &height);
      glfwWaitEvents();
    }
    
    (void)m_logDev.waitForFences(*m_inFlightFences[m_curFrame], vk::True, UINT64_MAX);
    
    m_logDev.waitIdle();
    
    cleanupSC();
    
    createSwapchain(*m_wnd);
    createImgViews();
    createGraphPipeline();
    size_t imageCount = m_sc.getImgs().size();
    m_imagesInFlight.assign(imageCount, nullptr);
  }
  
  void VulkanRenderer::cleanupSC() {
    m_pipeline.getPipeline().clear();
    m_pipeline.getPipLayout().clear();
    m_imgViews.clear();
    m_sc.getSC() = nullptr;
  }
  
  void VulkanRenderer::framebufferResizeCallback(GLFWwindow* wnd, int w, int h) {
    auto renderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(wnd));
    if (renderer) {
      renderer->framebufferResized = true;
    }
  }
  
  std::expected<uint32_t, std::string> VulkanRenderer::findMemType(uint32_t typeFilter, vk::MemoryPropertyFlags props) {
    
    vk::PhysicalDeviceMemoryProperties memProps = m_physDev.getMemoryProperties();
    for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
      if((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
        return i;
      }
    }
    
    return std::unexpected("no suitable memory type");
    
  }
  
  bool VulkanRenderer::createBuf(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memProps, vk::raii::Buffer& buf, vk::raii::DeviceMemory& mem) {
    vk::BufferCreateInfo info{
      .size = size,
      .usage = usage,
      .sharingMode = vk::SharingMode::eExclusive
    };
    
    {
      auto res = m_logDev.createBuffer(info);
      if(!res) {
        Logger::error("Failed to create staging buffer: {}", vk::to_string(res.error()));
        return false;
      }
      buf = std::move(res.value());
    }
    
    vk::MemoryRequirements memReq = buf.getMemoryRequirements();
    uint32_t typeStaging = 0;
    {
      auto res = findMemType(memReq.memoryTypeBits, memProps);
      if(!res) {
        Logger::error("Failed to find suitable memory type");
        return false;
      }
      typeStaging = res.value();
    }
    
    vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memReq.size,
      .memoryTypeIndex = typeStaging
    };
    
    {
      auto res = m_logDev.allocateMemory(allocInfo);
      if(!res) {
        Logger::error("Failed to allocate buffer memory: {}", vk::to_string(res.error()));
        return false;
      }
      mem = std::move(res.value());
    }
    
    buf.bindMemory(*mem, 0);
    return true;
  }
  
  bool VulkanRenderer::copyBuffer(vk::raii::Buffer& srcBuf, vk::raii::Buffer& dstBuf, vk::DeviceSize size) {
    vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = m_cmdPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1
    };
    vk::raii::CommandBuffer comCopyBuf{nullptr};
    {
      auto res = m_logDev.allocateCommandBuffers(allocInfo);
      if(!res) {
        Logger::error("Failed to allocate command buffer: {}", vk::to_string(res.error()));
        return false;
      }
      comCopyBuf = std::move(res.value().front());
    }
    comCopyBuf.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    comCopyBuf.copyBuffer(srcBuf, dstBuf, vk::BufferCopy(0, 0, size));
    comCopyBuf.end();
    
    m_graphQ.submit(vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &*comCopyBuf}, nullptr);
    m_graphQ.waitIdle();
    
    return true;
  }
  
  void VulkanRenderer::updUBO() {
    static auto startTime = std::chrono::high_resolution_clock::now();
    
    auto curTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(curTime - startTime).count();
    
    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.f), time * glm::radians(30.f), glm::vec3(0.f, 0.f, 1.f));
    ubo.view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
    ubo.proj = glm::perspective(glm::radians(45.f), static_cast<float>(m_sc.getExtent().width) / static_cast<float>(m_sc.getExtent().height), 0.1f, 10.f);
    
    ubo.proj[1][1] *= -1;
    
    memcpy(m_uniformBufsMapped[m_curFrame], &ubo, sizeof(ubo));
  }
  
  //====================================================================================================
  
  bool VulkanRenderer::init(Window& wnd) {
    
    m_wnd = &wnd;
    
    if(    !createInstance()
        || !setupDM()
        || !createSurf(wnd)
        || !pickPhysDev()
        || !createLogDev()
        || ! createSwapchain(wnd)
        || !createImgViews()
        || !createGraphPipeline()
        || !createCmdPool()
        || !createVBuf()
        || !createIBuf()
        || !createUBufs()
        || !createDescPool()
        || !createDescSets()
        || !createCmdBufs()
        || !createSyncObjs()
        
    ) return false;
    
    return true;
  }
  
  bool VulkanRenderer::createInstance() {
    
    constexpr vk::ApplicationInfo appInfo{
      .pApplicationName = "Hello Vulkan",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "NOENGINE",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = vk::ApiVersion14
    };
    
    std::vector<char const*> reqLayers;
    if(enableVaildLayers) {
      reqLayers.assign(m_validLayers.begin(), m_validLayers.end());
    }
    auto layerProps = m_ctx.enumerateInstanceLayerProperties();
    if(std::ranges::any_of(reqLayers, [&layerProps](auto const& reqLayer) {
      return std::ranges::none_of(layerProps, [reqLayer](auto const& layerProp) {
        return strcmp(layerProp.layerName, reqLayer) == 0;
      });
    })) {
      Logger::error("One or more required layers are not supported");
      return false;
    }
    
    auto glfwExtensions = getReqExtensions();
    auto extensionProps = m_ctx.enumerateInstanceExtensionProperties();
    for(auto const& ext :glfwExtensions) {
      if(std::ranges::none_of(extensionProps, [ext](auto const& extensionProp) {
        return strcmp(extensionProp.extensionName, ext) == 0;
      })) {
        Logger::error("Required GLFW extension not supported: {}", ext);
        return false;
      }
    }
    
    vk::InstanceCreateInfo createInfo{
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = static_cast<uint32_t>(reqLayers.size()),
      .ppEnabledLayerNames = reqLayers.data(),
      .enabledExtensionCount = static_cast<uint32_t>(glfwExtensions.size()),
      .ppEnabledExtensionNames = glfwExtensions.data()
    };
    
    auto res = m_ctx.createInstance(createInfo);
    if(res) {
      m_inst = std::move(res.value());
    }
    else {
      vk::Result errCode = res.error();
      Logger::error("Failed to create instance: {}", vk::to_string(errCode));
      return false;
    }
    
    Logger::debug("Vulkan instance created successfully");
    return true;
  }
  
  bool VulkanRenderer::setupDM() {
    
    if(!enableVaildLayers) return true;
    
    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(  vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                                                        | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                                                        | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT mesTypeFlags(   vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                                                    | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                                                    | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    vk::DebugUtilsMessengerCreateInfoEXT createInfo{
      .messageSeverity = severityFlags,
      .messageType = mesTypeFlags,
      .pfnUserCallback = &debugCallback
    };
    auto res = m_inst.createDebugUtilsMessengerEXT(createInfo);
    if(res) {
      m_debMesser = std::move(res.value());
      return true;
    }
    else {
      vk::Result errCode = res.error();
      Logger::error("Failed to create debug messenger: {}", vk::to_string(errCode));
      return false;
    }
    
  }
  
  bool VulkanRenderer::pickPhysDev() {
    
    auto res = m_inst.enumeratePhysicalDevices();
    if(res) {
      
      std::vector<vk::raii::PhysicalDevice> devs = res.value();
      if(devs.empty()) {
        Logger::error("Failed to find GPUs with Vulkan support");
        return false;
      }
      
      const auto devIter = std::ranges::find_if(devs,
        [&](auto const& dev) {
          auto queueFams = dev.getQueueFamilyProperties();
          bool isSuitable = dev.getProperties().apiVersion >= VK_API_VERSION_1_3;
          const auto qfpIter = std::ranges::find_if(queueFams,
            [](vk::QueueFamilyProperties const& qfp) {
              return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
            }
          );
          isSuitable = isSuitable && (qfpIter != queueFams.end());
          
          auto extensions = dev.enumerateDeviceExtensionProperties();
          bool found = true;
          for(auto const& ext : m_devExtensions) {
            auto extIter = std::ranges::find_if(extensions, [ext](auto const& extension) {return strcmp(extension.extensionName, ext) == 0;});
            found = found && extIter != extensions.end();
          }
          isSuitable = isSuitable && found;
          
          auto features = dev.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
          isSuitable =   isSuitable
                      && features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering
                      && features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;
          
          if(isSuitable) {
            m_physDev = dev;
          }
          
          printDev();
          
          return isSuitable;
        }
      );
      
      if(devIter  == devs.end()) {
        Logger::error("Failed to find a suitable GPU");
        return false;
      }
      
      return true;
    }
    else {
      vk::Result errCode = res.error();
      Logger::error("Failed to enumerate physical devices: {}", vk::to_string(errCode));
      return false;
    }
  }
  
  bool VulkanRenderer::createLogDev() {
    std::vector<vk::QueueFamilyProperties> qfProps = m_physDev.getQueueFamilyProperties();
    
    auto graphQFP = std::ranges::find_if(qfProps,
      [](auto const& qfp) {
        return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
      }
    );
    if(graphQFP == qfProps.end()) {
      Logger::error("No graphics queue family found");
      return false;
    }
    
    m_graphQI = static_cast<uint32_t>(std::distance(qfProps.begin(), graphQFP));
    m_presQI = m_physDev.getSurfaceSupportKHR(m_graphQI, *m_surf) ? m_graphQI : static_cast<uint32_t>(qfProps.size());
    if(m_presQI == qfProps.size()) {
      for(size_t i = 0; i < qfProps.size(); ++i) {
        if((qfProps[i].queueFlags & vk::QueueFlagBits::eGraphics) && m_physDev.getSurfaceSupportKHR(static_cast<uint32_t>(i), *m_surf)) {
          m_graphQI = static_cast<uint32_t>(i);
          m_presQI = m_graphQI;
          break;
        }
      }
      
      if(m_presQI == qfProps.size()) {
        for(size_t i = 0; i < qfProps.size(); ++i) {
          if(m_physDev.getSurfaceSupportKHR(static_cast<uint32_t>(i), *m_surf)) {
            m_presQI = static_cast<uint32_t>(i);
            break;
          }
        }
      }
    }
    if((m_graphQI == qfProps.size()) || (m_presQI == qfProps.size())) {
      Logger::error("Couldn't find a queue for graphics or present");
      return false;
    }
    
    vk::StructureChain<
      vk::PhysicalDeviceFeatures2,
      vk::PhysicalDeviceVulkan11Features,
      vk::PhysicalDeviceVulkan13Features,
      vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
    > featureChain = {
      {},
      {.shaderDrawParameters = true},
      {
        .synchronization2 = true,
        .dynamicRendering = true
      },
      {.extendedDynamicState = true}
    };
    
    std::set<uint32_t> uniqueQFIdcs = {m_graphQI, m_presQI};
    float qPrior = 0.f;
    std::vector<vk::DeviceQueueCreateInfo> qCreateInfos;
    for(uint32_t qFI : uniqueQFIdcs) {
      qCreateInfos.emplace_back(
        vk::DeviceQueueCreateInfo{
          .queueFamilyIndex = qFI,
          .queueCount = 1,
          .pQueuePriorities = &qPrior
        }
      );
    };
    
    vk::DeviceCreateInfo createInfo{
      .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
      .queueCreateInfoCount = static_cast<uint32_t>(qCreateInfos.size()),
      .pQueueCreateInfos = qCreateInfos.data(),
      .enabledExtensionCount = static_cast<uint32_t>(m_devExtensions.size()),
      .ppEnabledExtensionNames = m_devExtensions.data()
    };
    
    {
      auto res = m_physDev.createDevice(createInfo);
      if(!res) {
        Logger::error("Failed to create logical device: {}", vk::to_string(res.error()));
        return false;
      }
      m_logDev = std::move(res.value());
    }
      
    {
      auto res = m_logDev.getQueue(m_graphQI, 0);
      if(!res) {
        Logger::error("Failed to get graphics queue: {}", vk::to_string(res.error()));
        return false;
      }
      m_graphQ = res.value();
    }
      
    {
      auto res = m_logDev.getQueue(m_presQI, 0);
      if(!res) {
        Logger::error("Failed to get present queue: {}", vk::to_string(res.error()));
        return false;
      }
      m_presQ = res.value();
    }
    
    return true;
  }
  
  bool VulkanRenderer::createSurf(Window& wnd) {
    VkSurfaceKHR surf;
    if(glfwCreateWindowSurface(*m_inst, wnd.getWindow(), nullptr, &surf) != 0) {
      Logger::error("Failed to create window surface");
      return false;
    }
    
    m_surf = vk::raii::SurfaceKHR(m_inst, surf);
    return true;
  }
  
  bool VulkanRenderer::createSwapchain(Window& wnd) {
    if(!m_sc.init(wnd, m_physDev, m_logDev, m_surf, m_graphQI, m_presQI)) {
      return false;
    }
    
    m_imagesInFlight.resize(m_sc.getImgs().size(), nullptr);
    
    return true;
  }
  
  bool VulkanRenderer::createImgViews() {
    m_imgViews.clear();
    
    vk::ImageViewCreateInfo createInfo{
      .viewType = vk::ImageViewType::e2D,
      .format = m_sc.getFormat(),
    };
    createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;
    createInfo.components.r = vk::ComponentSwizzle::eIdentity;
    createInfo.components.g = vk::ComponentSwizzle::eIdentity;
    createInfo.components.b = vk::ComponentSwizzle::eIdentity;
    createInfo.components.a = vk::ComponentSwizzle::eIdentity;
    
    for(auto& img : m_sc.getImgs()) {
      createInfo.image = img;
      auto res = m_logDev.createImageView(createInfo);
      if(!res) {
        Logger::error("Failed to create image view: {}", vk::to_string(res.error()));
        return false;
      }
      m_imgViews.emplace_back(std::move(res.value()));
    }
    
    return true;
  }
  
  bool VulkanRenderer::createGraphPipeline() {
    if(!m_pipeline.init(m_logDev, m_sc)) {
      return false;
    }
    
    return true;
  }
  
  bool VulkanRenderer::createCmdPool() {
    
    vk::CommandPoolCreateInfo poolInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = m_graphQI
    };
    
    {
      auto res = m_logDev.createCommandPool(poolInfo);
      if(!res) {
        Logger::error("Failed to create command pool: {}", vk::to_string(res.error()));
        return false;
      }
      m_cmdPool = std::move(res.value());
    }
    
    return true;
  }
  
  bool VulkanRenderer::createVBuf() {
    
    vk::DeviceSize bufSize = sizeof(vertices[0]) * vertices.size();
    vk::raii::Buffer stagingBuf{nullptr};
    vk::raii::DeviceMemory stagingBufMem{nullptr};
    if(!createBuf(
      bufSize,
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      stagingBuf,
      stagingBufMem
    )) return false;
    
    void* dataStaging = stagingBufMem.mapMemory(0, bufSize);
    memcpy(dataStaging, vertices.data(), bufSize);
    stagingBufMem.unmapMemory();
    
    if(!createBuf(
      bufSize,
      vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      m_vertBuf,
      m_vertBufMem
    )) return false;
    
    copyBuffer(stagingBuf, m_vertBuf, bufSize);
    
    return true;
  }
  
  bool VulkanRenderer::createIBuf() {
    vk::DeviceSize bufSize = sizeof(indices[0]) * indices.size();
    
    vk::raii::Buffer stagingBuf{nullptr};
    vk::raii::DeviceMemory stagingBufMem{nullptr};
    if(!createBuf(
      bufSize,
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      stagingBuf,
      stagingBufMem
    )) return false;
    
    void* data = stagingBufMem.mapMemory(0, bufSize);
    memcpy(data, indices.data(), bufSize);
    stagingBufMem.unmapMemory();
    
    if(!createBuf(
      bufSize,
      vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      m_indBuf,
      m_indBufMem
    )) return false;
    
    copyBuffer(stagingBuf, m_indBuf, bufSize);
    
    return true;
  }
  
  bool VulkanRenderer::createUBufs() {
    m_uniformBufs.clear();
    m_uniformBufsMem.clear();
    m_uniformBufsMapped.clear();
    
    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      vk::DeviceSize bufSize = sizeof(UniformBufferObject);
      vk::raii::Buffer buf{nullptr};
      vk::raii::DeviceMemory bufMem{nullptr};
      if(!createBuf(
        bufSize,
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        buf,
        bufMem
      )) return false;
      m_uniformBufs.emplace_back(std::move(buf));
      m_uniformBufsMem.emplace_back(std::move(bufMem));
      m_uniformBufsMapped.emplace_back(m_uniformBufsMem[i].mapMemory(0, bufSize));
    }
    
    return true;
  }
  
  bool VulkanRenderer::createDescPool() {
    vk::DescriptorPoolSize poolSize{
      .type = vk::DescriptorType::eUniformBuffer,
      .descriptorCount = MAX_FRAMES_IN_FLIGHT
    };
    
    vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = MAX_FRAMES_IN_FLIGHT,
      .poolSizeCount = 1,
      .pPoolSizes = &poolSize
    };
    
    {
      auto res = m_logDev.createDescriptorPool(poolInfo);
      if(!res) {
        Logger::error("Failed to create descriptor pool: {}", vk::to_string(res.error()));
        return false;
      }
      m_descPool = std::move(res.value());
    }
    
    return true;
  }
  
  bool VulkanRenderer::createDescSets() {
    
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *(m_pipeline.getDescSetLayout()));
    vk::DescriptorSetAllocateInfo allocInfo{
      .descriptorPool = m_descPool,
      .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
      .pSetLayouts = layouts.data()
    };
    
    m_descSets.clear();
    {
      auto res = m_logDev.allocateDescriptorSets(allocInfo);
      if(!res) {
        Logger::error("Failed to allocate descriptor sets: {}", vk::to_string(res.error()));
        return false;
      }
      m_descSets = std::move(res.value());
    }

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      vk::DescriptorBufferInfo bufInfo{
        .buffer = m_uniformBufs[i],
        .offset = 0,
        .range = sizeof(UniformBufferObject)
      };
      vk::WriteDescriptorSet descWrite{
        .dstSet = m_descSets[i],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .pBufferInfo = &bufInfo
      };
      m_logDev.updateDescriptorSets(descWrite, {});
    }
    
    return true;
  }
  
  bool VulkanRenderer::createCmdBufs() {
    
    m_cmdBufs.clear();
    
    vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = m_cmdPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = MAX_FRAMES_IN_FLIGHT
    };
    
    {
      auto res = m_logDev.allocateCommandBuffers(allocInfo);
      if(!res) {
        Logger::error("Failed to allocate command buffer: {}", vk::to_string(res.error()));
        return false;
      }
      m_cmdBufs = std::move(res.value());
    }
    
    return true;
  }
  
  bool VulkanRenderer::createSyncObjs() {
    
    m_presCompleteSems.clear();
    m_renderFinishedSems.clear();
    m_inFlightFences.clear();
    
    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      {
        auto res = m_logDev.createSemaphore(vk::SemaphoreCreateInfo());
        if(!res) {
          Logger::error("Failed to create presentation semaphore: {}", vk::to_string(res.error()));
          return false;
        }
        m_presCompleteSems.emplace_back(std::move(res.value()));
      }
      {
        auto res = m_logDev.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
        if(!res) {
          Logger::error("Failed to create draw fence: {}", vk::to_string(res.error()));
          return false;
        }
        m_inFlightFences.emplace_back(std::move(res.value()));
      }
    }
    
    uint32_t imgCnt = m_sc.getImgs().size();
    for(uint32_t i = 0; i < imgCnt; ++i) {
      auto res = m_logDev.createSemaphore(vk::SemaphoreCreateInfo());
      if(!res) {
        Logger::error("Failed to create rendering semaphore: {}", vk::to_string(res.error()));
        return false;
      }
      m_renderFinishedSems.emplace_back(std::move(res.value()));
    }
    
    return true;
  }
  
  //====================================================================================================
  
  void VulkanRenderer::cleanup() {
    
  }
  
}; //V