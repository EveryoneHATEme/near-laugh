#include "near_laugh/application.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "core/engine.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "core/runtime_resources.hpp"

namespace near_laugh {

class Application::Impl {
 public:
  explicit Impl(RuntimeConfig config) {
    RuntimeResources resources = resolveRuntimeResources(config.resource_root);
    engine = std::make_unique<Engine>(config, std::move(resources), diagnostics);
  }

  ValidationDiagnostics diagnostics{};
  std::unique_ptr<Engine> engine{};
};

Application::Application(RuntimeConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

Application::~Application() = default;

void Application::run() {
  if (impl_->engine == nullptr) {
    throw std::logic_error("Application runtime has already stopped");
  }
  impl_->engine->run();
  impl_->engine.reset();
  if (impl_->diagnostics.errorCount() != 0) {
    throw std::runtime_error(
        "Vulkan validation reported " +
        std::to_string(impl_->diagnostics.errorCount()) +
        " error message(s) during runtime or teardown");
  }
}

}  // namespace near_laugh
