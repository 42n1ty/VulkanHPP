#pragma once

struct GLFWwindow;

namespace CS {
  
  class UILayer {
  public:
    
    UILayer();
    ~UILayer();
    
    void init(GLFWwindow* wnd);
    void shutdown();
    
    void beginFrame();
    void endFrame();
    
  };
  
}; //CS