#include "vk_renderer.hpp"

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
    
    transitionImageLayout(
      m_logDev,
      m_cmdPool,
      m_graphQ,
      m_sc.getImgs()[index],
      vk::ImageLayout::eUndefined,
      vk::ImageLayout::eColorAttachmentOptimal,
      {},
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::PipelineStageFlagBits2::eTopOfPipe,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput
    );
    
    vk::ImageMemoryBarrier2 depthBarrier{
      .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
      .srcAccessMask = {},
      .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
      .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      .oldLayout = vk::ImageLayout::eUndefined,
      .newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image = m_depthImg,
      .subresourceRange = {
        .aspectMask = vk::ImageAspectFlagBits::eDepth,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
      }
    };
    vk::DependencyInfo depthDependencyInfo = {
      .dependencyFlags = {},
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &depthBarrier
    };
    m_cmdBufs[m_curFrame].pipelineBarrier2(depthDependencyInfo);
    
    vk::ClearValue clearClr = vk::ClearColorValue(0.f, 0.f, 0.f, 1.f);
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
    
    vk::RenderingAttachmentInfo clrAttachmentInfo{
      .imageView = m_imgViews[index],
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clearClr
    };
    
    vk::RenderingAttachmentInfo dpthAttachmentInfo{
      .imageView = m_depthImgView,
      .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eDontCare,
      .clearValue = clearDepth
    };
    
    vk::RenderingInfo renderingInfo{
      .renderArea = {.offset = {0, 0}, .extent = m_sc.getExtent()},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &clrAttachmentInfo,
      .pDepthAttachment = &dpthAttachmentInfo
    };
    
    m_cmdBufs[m_curFrame].beginRendering(renderingInfo);
    
    m_cmdBufs[m_curFrame].setViewport(0, vk::Viewport(0.f, 0.f, static_cast<float>(m_sc.getExtent().width), static_cast<float>(m_sc.getExtent().height), 0.f, 1.f));
    m_cmdBufs[m_curFrame].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), m_sc.getExtent()));
    
    // m_cmdBufs[m_curFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getPipeline());
    m_cmdBufs[m_curFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_model->getPipLayout(), 0, *(m_perFrameDescSets[m_curFrame]), nullptr);
    
    m_model->draw(m_cmdBufs[m_curFrame]);
    // m_mesh.bind(m_cmdBufs[m_curFrame]);
    // m_cmdBufs[m_curFrame].drawIndexed(m_mesh.getIndexCount(), 1, 0, 0, 0);
    
    m_cmdBufs[m_curFrame].endRendering();
    
    transitionImageLayout(
      m_logDev,
      m_cmdPool,
      m_graphQ,
      m_sc.getImgs()[index],
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageLayout::ePresentSrcKHR,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      {},
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eBottomOfPipe
    );
    
    m_cmdBufs[m_curFrame].end();
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
    
    auto curTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(curTime - m_lastFrameTime).count();
    m_lastFrameTime = curTime;
    
    if(m_model->hasAnims()) {
      m_model->updAnim(deltaTime);
    }
    
    // MATRICES==================================================
    ObjectData objData{};
    // objData.model = glm::rotate(glm::mat4(1.f), deltaTime * glm::radians(30.f), glm::vec3(0.f, 0.f, 1.f));
    objData.model = glm::mat4(1.f);
    objData.model *= m_model->getNormMatrix();
    m_objectUBO.update(objData, m_curFrame);
    
    CameraData camData{};
    camData.view = glm::lookAt(glm::vec3(0.f, 2.f, 5.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));
    camData.proj = glm::perspective(glm::radians(45.f), static_cast<float>(m_sc.getExtent().width) / static_cast<float>(m_sc.getExtent().height), 0.1f, 10.f);
    camData.proj[1][1] *= -1; // reverse
    m_cameraUBO.update(camData, m_curFrame);
    
    BoneData boneData{};
    if(m_model->hasAnims()) {
      const auto& boneTransform = m_model->getBoneTransforms();
      if(!boneTransform.empty()) memcpy(boneData.bones, boneTransform.data(), boneTransform.size() * sizeof(glm::mat4));
    }
    m_bonesUBO.update(boneData, m_curFrame);
    // MATRICES==================================================
    
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
    // createGraphPipeline();
    
    createDepthRes();
    
    size_t imageCount = m_sc.getImgs().size();
    m_imagesInFlight.assign(imageCount, nullptr);
  }
  
  void VulkanRenderer::cleanupSC() {
    // m_pipeline.getPipeline().clear();
    // m_pipeline.getPipLayout().clear();
    m_imgViews.clear();
    m_sc.getSC() = nullptr;
  }
  
  void VulkanRenderer::framebufferResizeCallback(GLFWwindow* wnd, int w, int h) {
    auto renderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(wnd));
    if (renderer) {
      renderer->framebufferResized = true;
    }
  }
  
  //====================================================================================================
  
  bool VulkanRenderer::init(Window& wnd) {
    
    m_wnd = &wnd;
    m_lastFrameTime = std::chrono::high_resolution_clock::now();
    
    if(    !createInstance()
        || !setupDM()
        || !createSurf(wnd)
        || !pickPhysDev()
        || !createLogDev()
        || !createSwapchain(wnd)
        || !createImgViews()
        || !createDescSetLayouts()
        || !createCmdPool()
        
        || !createUBO()
        
        || !createDepthRes()
        
        || !createDescPool()
        || !createModel()
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
                      && features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState
                      && features.get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy;
          
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
      {
        .features{
          .samplerAnisotropy = true
        }
      },
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
    
    for(auto& img : m_sc.getImgs()) {
      vk::raii::ImageView temp{nullptr};
      
      if(!createImgView(img, m_sc.getFormat(), vk::ImageAspectFlagBits::eColor, temp, m_logDev)) {
        return false;
      }
      
      m_imgViews.emplace_back(std::move(temp));
    }
    
    return true;
  }
  
  bool VulkanRenderer::createDescSetLayouts() {
    // set=0
    std::array<vk::DescriptorSetLayoutBinding, 3> perFrameBindings = {
      // binding 0: camera ubo
      vk::DescriptorSetLayoutBinding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .pImmutableSamplers = nullptr
      },
      // binding 2: object ubo
      vk::DescriptorSetLayoutBinding{
        .binding = 2,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .pImmutableSamplers = nullptr
      },
      // binding 3: bones ubo
      vk::DescriptorSetLayoutBinding{
        .binding = 3,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .pImmutableSamplers = nullptr
      }
    };
    
    vk::DescriptorSetLayoutCreateInfo perFrameLayoutInfo{
      .flags = {},
      .bindingCount = perFrameBindings.size(),
      .pBindings = perFrameBindings.data()
    };
    
    {
      auto res = m_logDev.createDescriptorSetLayout(perFrameLayoutInfo);
      if(!res) {
        Logger::error("Failed to create descriptor set layout: {}", vk::to_string(res.error()));
        return false;
      }
      m_perFrameDescSetLayout = std::move(res.value());
    }
    
    // set=1
    // binding 0: texture sampler
    vk::DescriptorSetLayoutBinding textureBinding{
      .binding = 0,
      .descriptorType = vk::DescriptorType::eCombinedImageSampler,
      .descriptorCount = 1,
      .stageFlags = vk::ShaderStageFlagBits::eFragment,
      .pImmutableSamplers = nullptr
    };
    
    vk::DescriptorSetLayoutCreateInfo materialLayoutInfo {
      .bindingCount = 1,
      .pBindings = &textureBinding
    };
    
    {
      auto res = m_logDev.createDescriptorSetLayout(materialLayoutInfo);
      if(!res) {
        Logger::error("Failed to create descriptor set layout: {}", vk::to_string(res.error()));
        return false;
      }
      m_perMatDescSetLayout = std::move(res.value());
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
  
  bool VulkanRenderer::createUBO() {
    
    if(!m_cameraUBO.init(m_physDev, m_logDev)) {
      Logger::error("Failed to init camera ubo");
      return false;
    }
    if(!m_objectUBO.init(m_physDev, m_logDev)) {
      Logger::error("Failed to init camera ubo");
      return false;
    }
    if(!m_bonesUBO.init(m_physDev, m_logDev)) {
      Logger::error("Failed to init camera ubo");
      return false;
    }
    
    return true;
  }
  
  bool VulkanRenderer::createDepthRes() {
    
    vk::Format depthFormat;
    if(!findDepthFormat(depthFormat, m_physDev)) return false;
    
    if(!createImage(
          m_sc.getExtent().width,
          m_sc.getExtent().height,
          depthFormat,
          vk::ImageTiling::eOptimal,
          vk::ImageUsageFlagBits::eDepthStencilAttachment,
          vk::MemoryPropertyFlagBits::eDeviceLocal,
          m_depthImg,
          m_depthImgMem,
          m_physDev,
          m_logDev
        )
    ) return false;
    if(!createImgView(m_depthImg, depthFormat, vk::ImageAspectFlagBits::eDepth, m_depthImgView, m_logDev)) return false;
    
    return true;
  }
  
  bool VulkanRenderer::createDescPool() {
    
    std::array<vk::DescriptorPoolSize, 2> poolSize = {
      vk::DescriptorPoolSize{
        .type = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 3 * MAX_FRAMES_IN_FLIGHT // now three ubos per frame
      },
      vk::DescriptorPoolSize{
        .type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 100
      }
    };
    
    vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = 100 + MAX_FRAMES_IN_FLIGHT,
      .poolSizeCount = poolSize.size(),
      .pPoolSizes = poolSize.data()
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
  
  bool VulkanRenderer::createModel() {
    
    m_model = std::make_unique<VulkanModel>(
      false,
      m_physDev,
      m_logDev,
      m_sc,
      m_cmdPool,
      m_graphQ,
      m_perFrameDescSetLayout,
      m_perMatDescSetLayout,
      m_descPool
    );
    
    if(!m_model->load("../../assets/models/chest/source/MESH_Chest.fbx")) {
      Logger::error("Failed to load model");
      return false;
    }
    
    return true;
  }
  
  bool VulkanRenderer::createDescSets() {
    
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *(m_perFrameDescSetLayout));
    vk::DescriptorSetAllocateInfo allocInfo{
      .descriptorPool = m_descPool,
      .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
      .pSetLayouts = layouts.data()
    };
    
    m_perFrameDescSets.clear();
    {
      auto res = m_logDev.allocateDescriptorSets(allocInfo);
      if(!res) {
        Logger::error("Failed to allocate descriptor sets: {}", vk::to_string(res.error()));
        return false;
      }
      m_perFrameDescSets = std::move(res.value());
    }

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      vk::DescriptorBufferInfo cameraBufInfo{ // binding 0
        .buffer = m_cameraUBO.getUBufs()[i],
        .offset = 0,
        .range = sizeof(CameraData)
      };
      vk::DescriptorBufferInfo objectBufInfo{ // binding 2
        .buffer = m_objectUBO.getUBufs()[i],
        .offset = 0,
        .range = sizeof(ObjectData)
      };
      vk::DescriptorBufferInfo bonesBufInfo{ // binding 3
        .buffer = m_bonesUBO.getUBufs()[i],
        .offset = 0,
        .range = sizeof(BoneData)
      };
      
      
      std::array<vk::WriteDescriptorSet, 3> descWrites = {
        vk::WriteDescriptorSet {
          .dstSet = m_perFrameDescSets[i],
          .dstBinding = 0, // binding 0
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &cameraBufInfo
        },
        vk::WriteDescriptorSet {
          .dstSet = m_perFrameDescSets[i],
          .dstBinding = 2, // binding 2
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &objectBufInfo
        },
        vk::WriteDescriptorSet {
          .dstSet = m_perFrameDescSets[i],
          .dstBinding = 3, // binding 3
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &bonesBufInfo
        }
      };
      
      m_logDev.updateDescriptorSets(descWrites, {});
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