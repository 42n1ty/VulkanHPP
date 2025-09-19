#include "window.hpp"
#include "../tools/logger/logger.hpp"

namespace V {
  void Window::error_callback(int error, const char* description) {
    Logger::error("GLFW Error {}: {}", error, description);
  }

  void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
      self->m_Width = width;
      self->m_Height = height;
      // glViewport(0, 0, width, height);
      Logger::info("Window resized to {}x{}", width, height);
    }
  }

  Window::Window(int width, int height, const std::string& title, bool fullscreen)
    : m_Width(width), m_Height(height), isFullscreen(fullscreen) {
    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
      Logger::error("Failed to initialize GLFW!");
      return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_RESIZABLE, GL_FALSE); // temp for Vulkan
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // glfwWindowHint(GLFW_SAMPLES, 4);

    if(isFullscreen) {
      GLFWmonitor* mon = glfwGetPrimaryMonitor();
      if(!mon) {
        Logger::error("Failed to find primary monitor");
        isFullscreen = false;
        return;
      }
      const GLFWvidmode* mode = glfwGetVideoMode(mon);
      if(!mode) {
        Logger::error("Failed to get video mode");
        isFullscreen = false;
        return;
      }
      m_Width = mode->width;
      m_Height = mode->height;
    }
    if(isFullscreen) glfwWindowHint(GLFW_DECORATED, GL_FALSE);
    
    m_Window = glfwCreateWindow(m_Width, m_Height, title.c_str(), NULL, NULL);
    
    if (!m_Window) {
      glfwTerminate();
      Logger::error("Failed to create GLFW window!");
      return;
    }

    // glfwMakeContextCurrent(m_Window);
    
    // glfwSwapInterval(0); //v-sync
    
    glfwSetWindowUserPointer(m_Window, this);
    // glfwSetFramebufferSizeCallback(m_Window, framebuffer_size_callback);
    
    // if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    //   Logger::error("Failed to initialize GLAD!");
    //   std::abort();
    // }
    
    // glEnable(GL_FRAMEBUFFER_SRGB);
    
    // std::string vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
    // std::string renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    // std::string version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    
    // Logger::info("OpenGL Vendor: {};", vendor);
    // Logger::info("OpenGL Renderer: {};", renderer);
    // Logger::info("OpenGL Version: {}", version);

    // glViewport(0, 0, m_Width, m_Height);
    // glEnable(GL_DEPTH_TEST);
    // glEnable(GL_MULTISAMPLE);
  }

  Window::~Window() {
    glfwDestroyWindow(m_Window);
    glfwTerminate();
  }

  bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_Window);
  }

  void Window::swapBuffersAndPollEvents() {
    // glfwSwapBuffers(m_Window);
    glfwPollEvents();
  }

  float Window::getAspectRatio() const {
    if (m_Height == 0) return 1.0f;
    return static_cast<float>(m_Width) / static_cast<float>(m_Height);
  }
}