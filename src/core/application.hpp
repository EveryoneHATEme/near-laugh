#ifndef CORE_APPLICATION_HPP
#define CORE_APPLICATION_HPP

#include "core/engine.hpp"

class Application {
 public:
  Application() = default;
  ~Application() = default;

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;

  void gameLoop();

 private:
  Engine engine_;
};

#endif
