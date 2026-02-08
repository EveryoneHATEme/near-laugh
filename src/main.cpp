#include "core/application.hpp"

int main(int argc, char const *argv[]) {
  Application app;

  app.InitializeGPU();
  app.GameLoop();
  app.ReleaseGPU();
  return 0;
}
