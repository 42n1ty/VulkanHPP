#include "core/app.hpp"
#include "tools/logger/logger.hpp"
//====================================================================================================

//====================================================================================================
int main() {
  
  V::Application app;
  if(!app.init()) {
    V::Logger::error("Failed to init app");
    return -1;
  }
  app.run();
  
  return 0;
}