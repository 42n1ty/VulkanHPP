#include "vk_renderer.hpp"

#include "../../core/window.hpp"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "../../tools/logger/logger.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


const std::vector<V::Vertex> vertices = {
  {{-0.5f, -0.5f, 0.f}, {1.f, 0.f, 0.f}, {1.f, 0.f}},
  {{0.5f, -0.5f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
  {{0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 1.f}},
  {{-0.5f, 0.5f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
  
  {{-0.5f, -0.5f, -0.5f}, {1.f, 0.f, 0.f}, {1.f, 0.f}},
  {{0.5f, -0.5f, -0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
  {{0.5f, 0.5f, -0.5f}, {0.f, 0.f, 1.f}, {0.f, 1.f}},
  {{-0.5f, 0.5f, -0.5f}, {1.f, 1.f, 1.f}, {1.f, 1.f}}
};

const std::vector<uint32_t> indices = {
  0, 1, 2, 2, 3, 0,
  4, 5, 6, 6, 7, 4
};



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
    m_cmdBufs[m_curFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getPipeline());
    
    m_cmdBufs[m_curFrame].setViewport(0, vk::Viewport(0.f, 0.f, static_cast<float>(m_sc.getExtent().width), static_cast<float>(m_sc.getExtent().height), 0.f, 1.f));
    m_cmdBufs[m_curFrame].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), m_sc.getExtent()));
    
    m_mesh.bind(m_cmdBufs[m_curFrame]);
    m_cmdBufs[m_curFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline.getPipLayout(), 0, *(m_descSets[m_curFrame]), nullptr);
    m_cmdBufs[m_curFrame].drawIndexed(m_mesh.getIndexCount(), 1, 0, 0, 0);
    
    m_cmdBufs[m_curFrame].endRendering();
    
    transitionImageLayout(
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
    
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto curTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(curTime - startTime).count();
    
    glm::mat4 model = glm::rotate(glm::mat4(1.f), time * glm::radians(30.f), glm::vec3(0.f, 0.f, 1.f));
    glm::mat4 view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
    glm::mat4 proj = glm::perspective(glm::radians(45.f), static_cast<float>(m_sc.getExtent().width) / static_cast<float>(m_sc.getExtent().height), 0.1f, 10.f);
    
    m_vubo.updUBO(model, view, proj, m_curFrame);
    
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
    
    createDepthRes();
    
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
  
  bool VulkanRenderer::createImage(
    uint32_t w,
    uint32_t h,
    vk::Format format,
    vk::ImageTiling tiling,
    vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags props,
    vk::raii::Image& image,
    vk::raii::DeviceMemory& imageMem
  ) {
    
    vk::ImageCreateInfo imgInfo{
      .imageType = vk::ImageType::e2D,
      .format = format,
      .extent = vk::Extent3D{w, h, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = vk::SampleCountFlagBits::e1,
      .tiling = tiling,
      .usage = usage,
      .sharingMode = vk::SharingMode::eExclusive,
      .initialLayout = vk::ImageLayout::eUndefined
    };
    
    {
      auto res = m_logDev.createImage(imgInfo);
      if(!res) {
        Logger::error("Failed to create image: {}", vk::to_string(res.error()));
        return false;
      }
      image = std::move(res.value());
    }
    
    vk::MemoryRequirements memReq = image.getMemoryRequirements();
    uint32_t typeIndex = 0;
    {
      auto res = findMemType(memReq.memoryTypeBits, props, m_physDev);
      if(!res) {
        Logger::error("Failed to find suitable memory type");
        return false;
      }
      typeIndex = res.value();
    }
    vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memReq.size,
      .memoryTypeIndex = typeIndex
    };
    
    {
      auto res = m_logDev.allocateMemory(allocInfo);
      if(!res) {
        Logger::error("Failed to allocate image memory: {}", vk::to_string(res.error()));
        return false;
      }
      imageMem = std::move(res.value());
    }
    image.bindMemory(imageMem, 0);
    
    return true;
  }
  
  bool VulkanRenderer::transitionImageLayout(
    const vk::Image& image,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout,
    vk::AccessFlags2 srcAccessMask,
    vk::AccessFlags2 dstAccessMask,
    vk::PipelineStageFlags2 srcStageMask,
    vk::PipelineStageFlags2 dstStageMask,
    vk::raii::CommandBuffer* cmdBuf
  ) {
    
    vk::raii::CommandBuffer tempCmdBuf{nullptr};
    bool useTemp = (cmdBuf == nullptr);
    
    if(useTemp) {
      if(!beginSingleTimeComs(tempCmdBuf, m_logDev, m_cmdPool)) return false;
      cmdBuf = &tempCmdBuf;
    }

    vk::ImageMemoryBarrier2 barrier{
      .srcStageMask = srcStageMask,
      .srcAccessMask = srcAccessMask,
      .dstStageMask = dstStageMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
      }
    };

    // Обработка специальных случаев переходов
    if(oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
      barrier.srcAccessMask = {};
      barrier.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
      barrier.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
      barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    } 
    else if(oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
      barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
      barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
      barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
      barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
    }
    else if(oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
      barrier.srcAccessMask = srcAccessMask;
      barrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
      barrier.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
      barrier.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    }
    else if(oldLayout == vk::ImageLayout::eColorAttachmentOptimal && newLayout == vk::ImageLayout::ePresentSrcKHR) {
      barrier.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
      barrier.dstAccessMask = {};
      barrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
      barrier.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe;
    }
    else {
      Logger::error("Unsupported layout transition from {} to {}", vk::to_string(oldLayout), vk::to_string(newLayout));
      return false;
    }

    vk::DependencyInfo dependInfo{
      .dependencyFlags = {},
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier
    };

    cmdBuf->pipelineBarrier2(dependInfo);

    // Завершаем временный командный буфер если он использовался
    if(useTemp) {
      if(!endSingleTimeComs(tempCmdBuf, m_graphQ)) return false;
    }
    
    return true;
  }
  
  bool VulkanRenderer::copyBufToImg(const vk::raii::Buffer& buf, vk::raii::Image& img, uint32_t w, uint32_t h) {
    vk::raii::CommandBuffer cmdBuf{nullptr};
    if(!beginSingleTimeComs(cmdBuf, m_logDev, m_cmdPool)) return false;
    
    vk::BufferImageCopy region{
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
      .imageOffset = {0, 0, 0},
      .imageExtent = {w, h, 1}
    };
    
    cmdBuf.copyBufferToImage(buf, img, vk::ImageLayout::eTransferDstOptimal, {region});
    
    if(!endSingleTimeComs(cmdBuf, m_graphQ)) return false;
    
    return true;
  }
  
  bool VulkanRenderer::createImgView(const vk::Image& img, vk::Format format, vk::ImageAspectFlags aspectFlags, vk::raii::ImageView& iv) {
    
    vk::ImageViewCreateInfo viewInfo{
      .flags = {},
      .image = img,
      .viewType = vk::ImageViewType::e2D,
      .format = format,
      .components = {},
      .subresourceRange = { aspectFlags, 0, 1, 0, 1 }
    };
    
    auto res = m_logDev.createImageView(viewInfo);
    if(!res) {
      Logger::error("Failed to create image view: {}", vk::to_string(res.error()));
      return false;
    }
    iv = std::move(res.value());
    
    return true;
  }
  
  bool VulkanRenderer::findSupFormat(
    vk::Format& format,
    const std::vector<vk::Format>& candidates,
    vk::ImageTiling tiling,
    vk::FormatFeatureFlags features
  ) {
    
    for(const auto frmt : candidates) {
      vk::FormatProperties props = m_physDev.getFormatProperties(frmt);
      
      if(tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
        format = frmt;
        return true;
      }
      if(tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
        format = frmt;
        return true;
      }
    }
    
    Logger::error("Failed to find supported format");
    return false;
  }
  
  bool VulkanRenderer::findDepthFormat(vk::Format& format) {
    
    if(!findSupFormat(
          format,
          { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
          vk::ImageTiling::eOptimal,
          vk::FormatFeatureFlagBits::eDepthStencilAttachment
        )
    ) return false;
    
    return true;
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
        || !createDescSetLayout()
        || !createGraphPipeline()
        || !createCmdPool()
        
        || !createUBO()
        || !createMesh()
        
        || !createDepthRes()
        || !createTextureImg()
        || !createTextureImgView()
        || !createTextureSampler()
        
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
      
      if(!createImgView(img, m_sc.getFormat(), vk::ImageAspectFlagBits::eColor, temp)) {
        return false;
      }
      
      m_imgViews.emplace_back(std::move(temp));
    }
    
    return true;
  }
  
  bool VulkanRenderer::createDescSetLayout() {
    
    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
      vk::DescriptorSetLayoutBinding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .pImmutableSamplers = nullptr
      },
      vk::DescriptorSetLayoutBinding{
        .binding = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr
      }
    };
    
    vk::DescriptorSetLayoutCreateInfo layoutInfo{
      .flags = {},
      .bindingCount = bindings.size(),
      .pBindings = bindings.data()
    };
    
    {
      auto res = m_logDev.createDescriptorSetLayout(layoutInfo);
      if(!res) {
        Logger::error("Failed to create descriptor set layout: {}", vk::to_string(res.error()));
        return false;
      }
      m_descSetLayout = std::move(res.value());
    }
    
    return true;
  }
  
  bool VulkanRenderer::createGraphPipeline() {
    
    vk::Format depthFormat;
    if(!findDepthFormat(depthFormat)) return false;
    
    if(!m_pipeline.init(m_logDev, m_sc, m_descSetLayout, depthFormat)) {
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
  
  bool VulkanRenderer::createUBO() {
    
    if(!m_vubo.init(m_physDev, m_logDev)) {
      Logger::error("Failed to init ubo");
      return false;
    }
    
    return true;
  }
  
  bool VulkanRenderer::createMesh() {
    
    if(!m_mesh.init(vertices, indices, m_physDev, m_logDev, m_cmdPool, m_graphQ)) {
      Logger::error("Failed to init mesh");
      return false;
    }
    
    return true;
  }
  
  bool VulkanRenderer::createDepthRes() {
    
    vk::Format depthFormat;
    if(!findDepthFormat(depthFormat)) return false;
    
    if(!createImage(
          m_sc.getExtent().width,
          m_sc.getExtent().height,
          depthFormat,
          vk::ImageTiling::eOptimal,
          vk::ImageUsageFlagBits::eDepthStencilAttachment,
          vk::MemoryPropertyFlagBits::eDeviceLocal,
          m_depthImg,
          m_depthImgMem
        )
    ) return false;
    if(!createImgView(m_depthImg, depthFormat, vk::ImageAspectFlagBits::eDepth, m_depthImgView)) return false;
    
    return true;
  }
  
  bool VulkanRenderer::createTextureImg() {
    
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("../../assets/textures/txtr.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imgSize = texWidth * texHeight * 4;
    
    if(!pixels) {
      Logger::error("Failed to load texture image");
      return false;
    }
    
    vk::raii::Buffer stagingBuf({nullptr});
    vk::raii::DeviceMemory stagingBufMem({nullptr});
    
    if(!createBuf(
      imgSize,
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      stagingBuf,
      stagingBufMem,
      m_physDev,
      m_logDev
    )) return false;
    void* data = stagingBufMem.mapMemory(0, imgSize);
    memcpy(data, pixels, imgSize);
    stagingBufMem.unmapMemory();
    stbi_image_free(pixels);
    
    if(!createImage(
      texWidth,
      texHeight,
      vk::Format::eR8G8B8A8Srgb,
      vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      m_texImg,
      m_texImgMem
    )) return false;
    
    if(!transitionImageLayout(m_texImg, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal)) return false;
    if(!copyBufToImg(stagingBuf, m_texImg, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight))) return false;
    if(!transitionImageLayout(m_texImg, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal)) return false;
    
    return true;
  }
  
  bool VulkanRenderer::createTextureImgView() {
    
    if(!createImgView(*m_texImg, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, m_texImgView)) {
      return false;
    }
    
    return true;
  }
  
  bool VulkanRenderer::createTextureSampler() {
    
    vk::PhysicalDeviceProperties props = m_physDev.getProperties();
    
    vk::SamplerCreateInfo samplerInfo{
      .flags = {},
      .magFilter = vk::Filter::eLinear,
      .minFilter = vk::Filter::eLinear,
      .mipmapMode = vk::SamplerMipmapMode::eLinear,
      .addressModeU = vk::SamplerAddressMode::eRepeat,
      .addressModeV = vk::SamplerAddressMode::eRepeat,
      .addressModeW = vk::SamplerAddressMode::eRepeat,
      .mipLodBias = 0.f,
      .anisotropyEnable = 1,
      .maxAnisotropy = props.limits.maxSamplerAnisotropy,
      .compareEnable = vk::False,
      .compareOp = vk::CompareOp::eAlways,
      .minLod = 0.f,
      .maxLod = 0.f,
      .borderColor = vk::BorderColor::eIntOpaqueBlack,
      .unnormalizedCoordinates = vk::False
    };
    
    auto res = m_logDev.createSampler(samplerInfo);
    if(!res) {
      Logger::error("Failed to create texture sampler: {}", vk::to_string(res.error()));
      return false;
    }
    m_texSampler = std::move(res.value());
    
    return true;
  }
  
  bool VulkanRenderer::createDescPool() {
    
    std::array<vk::DescriptorPoolSize, 2> poolSize = {
      vk::DescriptorPoolSize{
        .type = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT
      },
      vk::DescriptorPoolSize{
        .type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT
      }
    };
    
    vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = MAX_FRAMES_IN_FLIGHT,
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
  
  bool VulkanRenderer::createDescSets() {
    
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *(m_descSetLayout));
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
        .buffer = m_vubo.getUBufs()[i],
        .offset = 0,
        .range = sizeof(UniformBufferObject)
      };
      
      vk::DescriptorImageInfo imgInfo{
        .sampler = m_texSampler,
        .imageView = m_texImgView,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
      };
      
      std::array<vk::WriteDescriptorSet, 2> descWrites = {
        vk::WriteDescriptorSet {
          .dstSet = m_descSets[i],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &bufInfo
        },
        vk::WriteDescriptorSet {
          .dstSet = m_descSets[i],
          .dstBinding = 1,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eCombinedImageSampler,
          .pImageInfo = &imgInfo,
          .pBufferInfo = nullptr
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