#include "vk_swapchain.hpp"
#include "vk_pipeline.hpp"
#include "vk_vertex.hpp"
#include "../../tools/logger/logger.hpp"

namespace V {
  
  VulkanPipeline::VulkanPipeline() {
    
  }
  
  VulkanPipeline::~VulkanPipeline() {
    
  }
  
  
  std::optional<std::vector<char>> VulkanPipeline::readFile(const std::string_view filename) {
    std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);
    
    if(!file.is_open()) {
      Logger::error("Failed to open file: {}", filename);
      return std::nullopt;
    }
    
    std::vector<char> buffer(file.tellg());
    
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    
    return buffer;
  }
  
  [[nodiscard]] std::optional<vk::raii::ShaderModule> VulkanPipeline::createShaderModule(const vk::raii::Device& dev, const std::vector<char>& code) const {
    vk::ShaderModuleCreateInfo createInfo{
      .codeSize = code.size() * sizeof(char),
      .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };
    
    auto res = dev.createShaderModule(createInfo);
    if(!res) {
      Logger::error("Failed to create shader module: {}", vk::to_string(res.error()));
      return std::nullopt;
    }
    auto shaderModule = std::move(res.value());
    
    return shaderModule;
  }
  
  bool VulkanPipeline::init(const vk::raii::Device& logDev, VulkanSwapchain& sc) {
    
    std::vector<char> shaderCode;
    vk::raii::ShaderModule shaderModule{nullptr};
    
    {
      auto res = readFile("../../assets/shaders/shader.spv");
      if(!res) {
        return false;
      }
      shaderCode = res.value();
    }
      
    {
      auto res = createShaderModule(logDev, shaderCode);
      if(!res) {
        return false;
      }
      shaderModule = std::move(res.value());
    }
    
    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
      .stage = vk::ShaderStageFlagBits::eVertex,
      .module = shaderModule,
      .pName = "vertMain"
    };
    
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
      .stage = vk::ShaderStageFlagBits::eFragment,
      .module = shaderModule,
      .pName = "fragMain"
    };
    
    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    
    auto bindingDesc = Vertex::getBindingDescription();
    auto attrDesc = Vertex::getAttribDescription();
    vk::PipelineVertexInputStateCreateInfo vertInputInfo{
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &bindingDesc,
      .vertexAttributeDescriptionCount = attrDesc.size(),
      .pVertexAttributeDescriptions = attrDesc.data()
    };
    
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
      .topology = vk::PrimitiveTopology::eTriangleList
    };
    
    std::vector<vk::DynamicState> dynStates = {
      vk::DynamicState::eViewport,
      vk::DynamicState::eScissor
    };
    
    vk::PipelineDynamicStateCreateInfo dynamicState{
      .dynamicStateCount = static_cast<uint32_t>(dynStates.size()),
      .pDynamicStates = dynStates.data()
    };
    
    vk::PipelineViewportStateCreateInfo viewportState{
      .viewportCount = 1,
      .scissorCount = 1
    };
    
    vk::PipelineRasterizationStateCreateInfo rasterizer{
      .depthClampEnable = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eBack,
      .frontFace = vk::FrontFace::eClockwise,
      .depthBiasEnable = vk::False,
      .depthBiasSlopeFactor = 1.f,
      .lineWidth = 1.f
    };
    
    vk::PipelineMultisampleStateCreateInfo multisampling{
      .rasterizationSamples = vk::SampleCountFlagBits::e1,
      .sampleShadingEnable = vk::False
    };
    
    vk::PipelineColorBlendAttachmentState clrBlendAttachment{
      .blendEnable = vk::True,
      .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
      .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
      .colorBlendOp = vk::BlendOp::eAdd,
      .srcAlphaBlendFactor = vk::BlendFactor::eOne,
      .dstAlphaBlendFactor = vk::BlendFactor::eZero,
      .alphaBlendOp = vk::BlendOp::eAdd,
      .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };
    
    vk::PipelineColorBlendStateCreateInfo clrBlending{
      .logicOpEnable = vk::False,
      .logicOp = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments = & clrBlendAttachment
    };
    
    vk::PipelineLayoutCreateInfo pipLayoutInfo{
      .setLayoutCount = 0,
      .pushConstantRangeCount = 0
    };
    
    {
      auto res = logDev.createPipelineLayout(pipLayoutInfo);
      if(!res) {
        Logger::error("Failed to create pipeline layout: {}", vk::to_string(res.error()));
        return false;
      }
      m_pipelineLayout = std::move(res.value());
    }
    
    vk::PipelineRenderingCreateInfo pipRenderCreateInfo{
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &sc.getFormat()
    };
    
    
    vk::GraphicsPipelineCreateInfo pipInfo{
      .pNext = &pipRenderCreateInfo,
      .stageCount = 2,
      .pStages = shaderStages,
      .pVertexInputState = &vertInputInfo,
      .pInputAssemblyState = &inputAssembly,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pColorBlendState = &clrBlending,
      .pDynamicState = &dynamicState,
      .layout = m_pipelineLayout,
      .renderPass = nullptr
    };
      
    {
      auto res = logDev.createGraphicsPipeline(nullptr, pipInfo);
      if(!res) {
        Logger::error("Failed to create graphics pipeline: {}", vk::to_string(res.error()));
        return false;
      }
      m_pipeline = std::move(res.value());
    }
    
    return true;
  }
  
}; //V