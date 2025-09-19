#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
// #include "glad/glad.h"
#include <string>
#include <functional>

namespace V {
  class Window {
  public:
    Window(int width, int height, const std::string& title, bool fullscreen);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void setShouldClose(bool flag) {glfwSetWindowShouldClose(m_Window, flag);}
    GLFWwindow* getWindow() const { return m_Window; }
    void swapBuffersAndPollEvents();

    float getAspectRatio() const;
    int m_Width;
    int m_Height;

  private:
    GLFWwindow* m_Window;
    bool isFullscreen;

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void error_callback(int error, const char* description);
  };
  
}; //V