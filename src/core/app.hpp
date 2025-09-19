#pragma once

#include "window.hpp"
#include "../renderer/vulkan/vk_renderer.hpp"
#include <chrono>

namespace V {
	class Application {
  public:
    Application();
    ~Application();

    bool init();
    void run();

  private:
    
    std::unique_ptr<Window> m_Window = nullptr;
    std::unique_ptr<VulkanRenderer> m_renderer = nullptr;
    
    void processInput(GLFWwindow* wnd, const float dT);
    
  };

}; //V