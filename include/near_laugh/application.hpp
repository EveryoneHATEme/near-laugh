#ifndef NEAR_LAUGH_APPLICATION_HPP
#define NEAR_LAUGH_APPLICATION_HPP

#include <memory>

#include "near_laugh/runtime_config.hpp"

namespace near_laugh {

class Application {
 public:
  explicit Application(RuntimeConfig config);
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;

  void run();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace near_laugh

#endif
