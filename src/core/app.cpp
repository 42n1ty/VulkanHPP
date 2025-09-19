#include "app.hpp"
#include "../tools/logger/logger.hpp"

namespace V {
  
	Application::Application() {
    Logger::info("Application created");
  }

  Application::~Application() {
    Logger::info("Application shutting down.");
  }

  
  bool Application::init() {
    Logger::info("Application initializing...");
    
    // WINDOW==================================================
    m_Window = std::make_unique<Window>(1280, 720, "Crimson Souls", false);
    // WINDOW==================================================
    
    // VULKAN==================================================
    m_renderer = std::make_unique<VulkanRenderer>();
    if(!m_renderer->init(*m_Window)) {
      return false;
    }
    
    // VULKAN==================================================
    
    glfwSetWindowUserPointer(m_Window->getWindow(), m_renderer.get());
    glfwSetFramebufferSizeCallback(m_Window->getWindow(), VulkanRenderer::framebufferResizeCallback);

    Logger::info("Application initialized successfully");
    return true;
  }
  
  void Application::run() {
    Logger::info("Entering main loop...");
    float lastFrameTime = 0.0f;

    while (!m_Window->shouldClose()) {
      float currentFrameTime = static_cast<float>(glfwGetTime());
      
      float deltaTime = currentFrameTime - lastFrameTime;
      lastFrameTime = currentFrameTime;
      
      processInput(m_Window->getWindow(), deltaTime);
      if(!m_renderer->drawFrame()) break;
      m_Window->swapBuffersAndPollEvents();
    }
    m_renderer->wait(); // wait for finishing work
  }
  
  void Application::processInput(GLFWwindow * wnd, const float dT) {
    static const float cSpd = 0.1f;
    static const float lSpd = 0.2f;
    
		if(glfwGetKey(wnd, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(wnd, true);
		}
    
	}
  
}; //V