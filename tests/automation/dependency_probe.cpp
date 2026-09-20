#include <imgui.h>
#include <imgui_te_engine.h>

#include <cstdio>

int main() {
  IMGUI_CHECKVERSION();
  ImGuiContext* context = ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(800, 600);
  io.DeltaTime = 1.0f / 60.0f;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
  ImGuiTestEngine* engine = ImGuiTestEngine_CreateContext();
  auto& engine_io = ImGuiTestEngine_GetIO(engine);
  engine_io.ConfigSavedSettings = false;
  engine_io.ConfigCaptureEnabled = false;
  ImGuiTestEngine_Start(engine, context);
  for (int frame = 0; frame != 3; ++frame) {
    ImGui::NewFrame();
    ImGui::Begin("Dependency lifecycle");
    ImGui::TextUnformatted("No window, GPU or physical input");
    ImGui::End();
    ImGui::Render();
  }
  ImGuiTestEngine_Stop(engine);
  ImGui::DestroyContext(context);
  ImGuiTestEngine_DestroyContext(engine);
  std::printf("PASS: ImGui %s (%d), Test Engine create/start/3 frames/stop/destroy\n",
              IMGUI_VERSION, IMGUI_VERSION_NUM);
}
