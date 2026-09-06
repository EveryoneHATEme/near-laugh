#include "editor/editor_renderer.hpp"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "core/platform/window.hpp"
#include "core/render/depth_attachment.hpp"
#include "core/render/graphics_pipeline.hpp"
#include "core/render/immutable_mesh_buffer.hpp"
#include "core/render/lighting_resources.hpp"
#include "core/render/prototype_scene.hpp"
#include "core/render/sampled_texture.hpp"
#include "core/render/static_model_loader.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "core/render/vulkan_context.hpp"
#include "core/render/vulkan_utils.hpp"
#include "core/testing/test_controls.hpp"
#include "core/world/prototype_level.hpp"
#include "editor/editor_overlay.hpp"

namespace {
struct SwapchainSupport {
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> formats{};
  std::vector<VkPresentModeKHR> present_modes{};
};

SwapchainSupport querySwapchainSupport(VkPhysicalDevice physical_device,
                                       VkSurfaceKHR surface) {
  SwapchainSupport support;
  requireVulkan(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                    physical_device, surface, &support.capabilities),
                "Query Vulkan surface capabilities");
  std::uint32_t count = 0;
  requireVulkan(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                                     &count, nullptr),
                "Query Vulkan surface format count");
  support.formats.resize(count);
  if (count != 0) {
    requireVulkan(vkGetPhysicalDeviceSurfaceFormatsKHR(
                      physical_device, surface, &count, support.formats.data()),
                  "Query Vulkan surface formats");
  }
  count = 0;
  requireVulkan(vkGetPhysicalDeviceSurfacePresentModesKHR(
                    physical_device, surface, &count, nullptr),
                "Query Vulkan presentation mode count");
  support.present_modes.resize(count);
  if (count != 0) {
    requireVulkan(
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device, surface, &count, support.present_modes.data()),
        "Query Vulkan presentation modes");
  }
  return support;
}

VkFormat selectDepthFormat(VkPhysicalDevice physical_device) {
  constexpr std::array<VkFormat, 3> candidates = {VK_FORMAT_D32_SFLOAT,
                                                  VK_FORMAT_D32_SFLOAT_S8_UINT,
                                                  VK_FORMAT_D24_UNORM_S8_UINT};
  std::vector<FormatFeatureSupport> support;
  support.reserve(candidates.size());
  for (const VkFormat format : candidates) {
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);
    support.push_back({format, properties.optimalTilingFeatures});
  }
  return chooseDepthFormat(support);
}
}  // namespace

class EditorRenderer::Impl {
 public:
  static constexpr std::size_t frames_in_flight = 2;

  struct FrameSlot {
    VkCommandPool command_pool{VK_NULL_HANDLE};
    VkCommandBuffer command_buffer{VK_NULL_HANDLE};
    VkSemaphore image_available{VK_NULL_HANDLE};
    VkFence completion{VK_NULL_HANDLE};
  };

  Impl(const Window& window, FramebufferExtent initial_extent,
       EditorRendererResources resources, ValidationDiagnostics& diagnostics);
  ~Impl();

  void beginUiFrame();
  void replaceDocument(const LevelDocument& level);
  void replaceTerrain(const LevelDocument& level);
  [[nodiscard]] std::size_t terrainReplacementCount() const noexcept {
    return terrain_replacement_count_;
  }
  void clearDocument();
  [[nodiscard]] FrameOutcome renderFrame(const FrameRequest& request);
  void requestSwapchainRecreation() noexcept { recreate_requested_ = true; }
  [[nodiscard]] bool validationEnabled() const noexcept {
    return context_.validationEnabled();
  }

 private:
  void createSwapchain(FramebufferExtent framebuffer);
  void cleanupSwapchain() noexcept;
  void recreateSwapchain(FramebufferExtent framebuffer);
  void createFrameSlots();
  void cleanupFrameSlots() noexcept;
  void initializeImGuiBackend();
  void rebuildImGuiPipeline();
  void shutdownImGuiBackend() noexcept;
  void recordFrame(VkCommandBuffer command_buffer, std::uint32_t image_index,
                   const FrameRequest& request);

  VulkanContext context_;
  EditorRendererResources resources_{};
  std::unique_ptr<SampledTexture> sampled_texture_{};
  std::unique_ptr<LightingResources> lighting_resources_{};
  std::unique_ptr<ImmutableMeshBuffer> world_mesh_{};
  std::unique_ptr<ImmutableMeshBuffer> chair_mesh_{};
  VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
  VkFormat swapchain_format_{VK_FORMAT_UNDEFINED};
  VkFormat depth_format_{VK_FORMAT_UNDEFINED};
  VkExtent2D swapchain_extent_{};
  std::vector<VkImage> swapchain_images_{};
  std::vector<VkImageView> swapchain_views_{};
  std::vector<std::unique_ptr<DepthAttachment>> depth_attachments_{};
  std::vector<VkSemaphore> render_finished_{};
  std::vector<VkFence> image_fences_{};
  std::vector<bool> image_initialized_{};
  std::unique_ptr<GraphicsPipeline> pipeline_{};
  std::array<FrameSlot, frames_in_flight> frames_{};
  std::size_t current_frame_{};
  std::uint32_t minimum_image_count_{2};
  bool recreate_requested_{};
  bool imgui_backend_initialized_{};
  std::size_t terrain_replacement_count_{};
};

EditorRenderer::EditorRenderer(const Window& window,
                               FramebufferExtent initial_extent,
                               EditorRendererResources resources,
                               ValidationDiagnostics& diagnostics)
    : impl_(std::make_unique<Impl>(window, initial_extent, std::move(resources),
                                   diagnostics)) {}

EditorRenderer::~EditorRenderer() = default;

void EditorRenderer::beginUiFrame() { impl_->beginUiFrame(); }

void EditorRenderer::replaceDocument(const LevelDocument& level) {
  impl_->replaceDocument(level);
}

void EditorRenderer::replaceTerrain(const LevelDocument& level) {
  impl_->replaceTerrain(level);
}

std::size_t EditorRenderer::terrainReplacementCount() const noexcept {
  return impl_->terrainReplacementCount();
}

void EditorRenderer::drawOverlays(std::span<const EditorOverlayLine> lines) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImDrawList* draw = ImGui::GetBackgroundDrawList();
  for (const auto& line : lines) {
    const auto point = [&](std::array<float, 2> p) {
      return ImVec2{viewport->Pos.x + p[0] * viewport->Size.x,
                    viewport->Pos.y + p[1] * viewport->Size.y};
    };
    draw->AddLine(
        point(line.first), point(line.second),
        IM_COL32(line.color[0], line.color[1], line.color[2], line.color[3]),
        1.5F);
  }
}

void EditorRenderer::clearDocument() { impl_->clearDocument(); }

FrameOutcome EditorRenderer::renderFrame(const FrameRequest& request) {
  if (!frameRequestCanSubmit(request)) {
    return FrameOutcome::Skipped;
  }
  if (!spotLightFrameIsValid(request.spot_light)) {
    throw std::invalid_argument(
        "EditorRenderer requires a valid spot-light frame");
  }
  return impl_->renderFrame(request);
}

void EditorRenderer::requestSwapchainRecreation() noexcept {
  impl_->requestSwapchainRecreation();
}

bool EditorRenderer::validationEnabled() const noexcept {
  return impl_->validationEnabled();
}

EditorRenderer::Impl::Impl(const Window& window,
                           FramebufferExtent initial_extent,
                           EditorRendererResources resources,
                           ValidationDiagnostics& diagnostics)
    : context_(window, diagnostics), resources_(std::move(resources)) {
  try {
    sampled_texture_ = std::make_unique<SampledTexture>(
        context_.device(), context_.physicalDevice(), context_.graphicsQueue(),
        context_.queueFamilies().graphics, resources_.surface_textures);
    depth_format_ = selectDepthFormat(context_.physicalDevice());
    createSwapchain(initial_extent);
    createFrameSlots();
    initializeImGuiBackend();
    recordLifecycleEvent("editor.renderer.created");
  } catch (...) {
    shutdownImGuiBackend();
    cleanupFrameSlots();
    pipeline_.reset();
    cleanupSwapchain();
    lighting_resources_.reset();
    sampled_texture_.reset();
    chair_mesh_.reset();
    world_mesh_.reset();
    throw;
  }
}

EditorRenderer::Impl::~Impl() {
  if (context_.device() != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(context_.device());
  }
  shutdownImGuiBackend();
  cleanupFrameSlots();
  pipeline_.reset();
  cleanupSwapchain();
  lighting_resources_.reset();
  sampled_texture_.reset();
  chair_mesh_.reset();
  world_mesh_.reset();
  recordLifecycleEvent("editor.renderer.destroyed");
}

void EditorRenderer::Impl::beginUiFrame() { ImGui_ImplVulkan_NewFrame(); }

void EditorRenderer::Impl::replaceDocument(const LevelDocument& level) {
  requireVulkan(vkDeviceWaitIdle(context_.device()),
                "Wait for editor frames before replacing the level");
  const std::vector<PositionColorVertex> world_vertices =
      buildPrototypeSceneVertices(level.terrain, level.solids,
                                  level.light_switch);
  const std::vector<PositionColorVertex> chair_vertices =
      loadStaticModelVertices(resources_.prototype_chair_model,
                              level.static_prop);
  std::unique_ptr<ImmutableMeshBuffer> world_mesh;
  if (!world_vertices.empty())
    world_mesh = std::make_unique<ImmutableMeshBuffer>(
        context_.device(), context_.physicalDevice(), world_vertices, "world");
  auto chair_mesh = std::make_unique<ImmutableMeshBuffer>(
      context_.device(), context_.physicalDevice(), chair_vertices, "chair");
  auto lighting_resources = std::make_unique<LightingResources>(
      context_.device(), context_.physicalDevice(), level.environment_light);
  auto pipeline = std::make_unique<GraphicsPipeline>(
      context_.device(), swapchain_format_, depth_format_,
      sampled_texture_->descriptorSetLayout(),
      sampled_texture_->descriptorSet(),
      lighting_resources->descriptorSetLayout(),
      lighting_resources->descriptorSet(), resources_.vertex_shader,
      resources_.fragment_shader);

  pipeline_ = std::move(pipeline);
  lighting_resources_ = std::move(lighting_resources);
  chair_mesh_ = std::move(chair_mesh);
  world_mesh_ = std::move(world_mesh);
  recordLifecycleEvent("editor.document-resources.replaced");
}

void EditorRenderer::Impl::clearDocument() {
  requireVulkan(vkDeviceWaitIdle(context_.device()),
                "Wait for editor frames before closing the level");
  pipeline_.reset();
  lighting_resources_.reset();
  chair_mesh_.reset();
  world_mesh_.reset();
  recordLifecycleEvent("editor.document-resources.cleared");
}

void EditorRenderer::Impl::replaceTerrain(const LevelDocument& level) {
  // Both frame slots can reference the shared world buffer. Keep the small,
  // unchanged solid stream beside terrain; chair, lights and pipeline survive.
  std::array<VkFence, frames_in_flight> fences{};
  for (std::size_t i = 0; i < frames_.size(); ++i)
    fences[i] = frames_[i].completion;
  requireVulkan(
      vkWaitForFences(context_.device(),
                      static_cast<std::uint32_t>(fences.size()), fences.data(),
                      VK_TRUE, std::numeric_limits<std::uint64_t>::max()),
      "Wait for editor terrain buffer readers");
  const auto vertices = buildPrototypeSceneVertices(level.terrain, level.solids,
                                                    level.light_switch);
  std::unique_ptr<ImmutableMeshBuffer> mesh;
  if (!vertices.empty())
    mesh = std::make_unique<ImmutableMeshBuffer>(
        context_.device(), context_.physicalDevice(), vertices, "world");
  world_mesh_ = std::move(mesh);
  ++terrain_replacement_count_;
  recordLifecycleEvent("editor.terrain-resources.replaced");
}

void EditorRenderer::Impl::initializeImGuiBackend() {
  if (forcedVulkanFailureAt("editor-imgui-backend")) {
    throw std::runtime_error("Forced editor ImGui Vulkan backend failure");
  }
  VkPipelineRenderingCreateInfo pipeline_rendering{};
  pipeline_rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  pipeline_rendering.colorAttachmentCount = 1;
  pipeline_rendering.pColorAttachmentFormats = &swapchain_format_;
  pipeline_rendering.depthAttachmentFormat = depth_format_;
  ImGui_ImplVulkan_InitInfo info{};
  info.ApiVersion = VK_API_VERSION_1_3;
  info.Instance = context_.instance();
  info.PhysicalDevice = context_.physicalDevice();
  info.Device = context_.device();
  info.QueueFamily = context_.queueFamilies().graphics;
  info.Queue = context_.graphicsQueue();
  info.DescriptorPoolSize = 32;
  info.MinImageCount = minimum_image_count_;
  info.ImageCount = static_cast<std::uint32_t>(swapchain_images_.size());
  info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  info.PipelineInfoMain.PipelineRenderingCreateInfo = pipeline_rendering;
  info.UseDynamicRendering = true;
  if (!ImGui_ImplVulkan_Init(&info)) {
    throw std::runtime_error("Dear ImGui Vulkan backend initialization failed");
  }
  imgui_backend_initialized_ = true;
  recordLifecycleEvent("editor.imgui-vulkan.created");
}

void EditorRenderer::Impl::shutdownImGuiBackend() noexcept {
  if (!imgui_backend_initialized_) {
    return;
  }
  ImGui_ImplVulkan_Shutdown();
  imgui_backend_initialized_ = false;
  recordLifecycleEvent("editor.imgui-vulkan.destroyed");
}

void EditorRenderer::Impl::rebuildImGuiPipeline() {
  VkPipelineRenderingCreateInfo pipeline_rendering{};
  pipeline_rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  pipeline_rendering.colorAttachmentCount = 1;
  pipeline_rendering.pColorAttachmentFormats = &swapchain_format_;
  pipeline_rendering.depthAttachmentFormat = depth_format_;
  ImGui_ImplVulkan_PipelineInfo pipeline_info{};
  pipeline_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  pipeline_info.PipelineRenderingCreateInfo = pipeline_rendering;
  ImGui_ImplVulkan_CreateMainPipeline(&pipeline_info);
}

void EditorRenderer::Impl::createSwapchain(FramebufferExtent framebuffer) {
  const SwapchainSupport support =
      querySwapchainSupport(context_.physicalDevice(), context_.surface());
  if (support.formats.empty() || support.present_modes.empty()) {
    throw std::runtime_error(
        "Swapchain creation failed: surface formats or presentation modes are "
        "empty");
  }
  const VkSurfaceFormatKHR surface_format =
      chooseSurfaceFormat(support.formats);
  const VkExtent2D extent = chooseSwapchainExtent(
      support.capabilities, {framebuffer.width, framebuffer.height});
  requireColorAttachmentSwapchainUsage(
      support.capabilities.supportedUsageFlags);
  const VkCompositeAlphaFlagBitsKHR composite_alpha =
      chooseCompositeAlpha(support.capabilities.supportedCompositeAlpha);
  std::uint32_t image_count = support.capabilities.minImageCount + 1;
  if (support.capabilities.maxImageCount != 0) {
    image_count = std::min(image_count, support.capabilities.maxImageCount);
  }
  minimum_image_count_ = std::max(2U, support.capabilities.minImageCount);

  const QueueFamilySelection families = context_.queueFamilies();
  const std::array<std::uint32_t, 2> family_indices = {families.graphics,
                                                       families.present};
  VkSwapchainCreateInfoKHR info{};
  info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  info.surface = context_.surface();
  info.minImageCount = image_count;
  info.imageFormat = surface_format.format;
  info.imageColorSpace = surface_format.colorSpace;
  info.imageExtent = extent;
  info.imageArrayLayers = 1;
  info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (families.sharesFamily()) {
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  } else {
    info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    info.queueFamilyIndexCount =
        static_cast<std::uint32_t>(family_indices.size());
    info.pQueueFamilyIndices = family_indices.data();
  }
  info.preTransform = support.capabilities.currentTransform;
  info.compositeAlpha = composite_alpha;
  info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  info.clipped = VK_TRUE;
  requireVulkan(
      vkCreateSwapchainKHR(context_.device(), &info, nullptr, &swapchain_),
      "Create Vulkan swapchain");

  image_count = 0;
  requireVulkan(vkGetSwapchainImagesKHR(context_.device(), swapchain_,
                                        &image_count, nullptr),
                "Query Vulkan swapchain image count");
  swapchain_images_.resize(image_count);
  requireVulkan(vkGetSwapchainImagesKHR(context_.device(), swapchain_,
                                        &image_count, swapchain_images_.data()),
                "Get Vulkan swapchain images");
  swapchain_views_.resize(image_count, VK_NULL_HANDLE);
  render_finished_.resize(image_count, VK_NULL_HANDLE);
  try {
    for (std::size_t index = 0; index < swapchain_images_.size(); ++index) {
      VkImageViewCreateInfo view_info{};
      view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      view_info.image = swapchain_images_[index];
      view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      view_info.format = surface_format.format;
      view_info.components = {
          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
      view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      view_info.subresourceRange.levelCount = 1;
      view_info.subresourceRange.layerCount = 1;
      requireVulkan(vkCreateImageView(context_.device(), &view_info, nullptr,
                                      &swapchain_views_[index]),
                    "Create Vulkan swapchain image view");
      VkSemaphoreCreateInfo semaphore_info{};
      semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      requireVulkan(vkCreateSemaphore(context_.device(), &semaphore_info,
                                      nullptr, &render_finished_[index]),
                    "Create per-image render-finished semaphore");
    }
    depth_attachments_.reserve(swapchain_images_.size());
    for (std::size_t index = 0; index < swapchain_images_.size(); ++index) {
      depth_attachments_.push_back(std::make_unique<DepthAttachment>(
          context_.device(), context_.physicalDevice(), extent, depth_format_));
    }
  } catch (...) {
    cleanupSwapchain();
    throw;
  }
  swapchain_format_ = surface_format.format;
  swapchain_extent_ = extent;
  image_fences_.assign(image_count, VK_NULL_HANDLE);
  image_initialized_.assign(image_count, false);
  std::cout << "Swapchain: format " << swapchain_format_ << ", extent "
            << swapchain_extent_.width << 'x' << swapchain_extent_.height
            << ", images " << image_count << '\n';
}

void EditorRenderer::Impl::cleanupSwapchain() noexcept {
  depth_attachments_.clear();
  for (const VkSemaphore semaphore : render_finished_) {
    if (semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(context_.device(), semaphore, nullptr);
    }
  }
  render_finished_.clear();
  for (const VkImageView view : swapchain_views_) {
    if (view != VK_NULL_HANDLE) {
      vkDestroyImageView(context_.device(), view, nullptr);
    }
  }
  swapchain_views_.clear();
  swapchain_images_.clear();
  image_fences_.clear();
  image_initialized_.clear();
  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(context_.device(), swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
  }
}

void EditorRenderer::Impl::recreateSwapchain(FramebufferExtent framebuffer) {
  if (framebuffer.isZero()) {
    return;
  }
  requireVulkan(vkDeviceWaitIdle(context_.device()),
                "Wait for Vulkan device before swapchain recreation");
  const VkFormat previous_format = swapchain_format_;
  cleanupSwapchain();
  createSwapchain(framebuffer);
  if (pipeline_ != nullptr && previous_format != swapchain_format_) {
    pipeline_ = std::make_unique<GraphicsPipeline>(
        context_.device(), swapchain_format_, depth_format_,
        sampled_texture_->descriptorSetLayout(),
        sampled_texture_->descriptorSet(),
        lighting_resources_->descriptorSetLayout(),
        lighting_resources_->descriptorSet(), resources_.vertex_shader,
        resources_.fragment_shader);
  }
  if (previous_format != swapchain_format_) {
    rebuildImGuiPipeline();
  }
  recreate_requested_ = false;
}

void EditorRenderer::Impl::createFrameSlots() {
  try {
    for (FrameSlot& frame : frames_) {
      VkCommandPoolCreateInfo pool_info{};
      pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      pool_info.queueFamilyIndex = context_.queueFamilies().graphics;
      pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
      requireVulkan(vkCreateCommandPool(context_.device(), &pool_info, nullptr,
                                        &frame.command_pool),
                    "Create per-frame Vulkan command pool");

      VkCommandBufferAllocateInfo allocate_info{};
      allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocate_info.commandPool = frame.command_pool;
      allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocate_info.commandBufferCount = 1;
      requireVulkan(vkAllocateCommandBuffers(context_.device(), &allocate_info,
                                             &frame.command_buffer),
                    "Allocate per-frame Vulkan command buffer");
      VkSemaphoreCreateInfo semaphore_info{};
      semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      requireVulkan(vkCreateSemaphore(context_.device(), &semaphore_info,
                                      nullptr, &frame.image_available),
                    "Create image-available semaphore");
      VkFenceCreateInfo fence_info{};
      fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
      requireVulkan(vkCreateFence(context_.device(), &fence_info, nullptr,
                                  &frame.completion),
                    "Create initially-signaled frame fence");
    }
  } catch (...) {
    cleanupFrameSlots();
    throw;
  }
}

void EditorRenderer::Impl::cleanupFrameSlots() noexcept {
  for (FrameSlot& frame : frames_) {
    if (frame.completion != VK_NULL_HANDLE) {
      vkDestroyFence(context_.device(), frame.completion, nullptr);
      frame.completion = VK_NULL_HANDLE;
    }
    if (frame.image_available != VK_NULL_HANDLE) {
      vkDestroySemaphore(context_.device(), frame.image_available, nullptr);
      frame.image_available = VK_NULL_HANDLE;
    }
    if (frame.command_pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(context_.device(), frame.command_pool, nullptr);
      frame.command_pool = VK_NULL_HANDLE;
      frame.command_buffer = VK_NULL_HANDLE;
    }
  }
}

void EditorRenderer::Impl::recordFrame(VkCommandBuffer command_buffer,
                                       std::uint32_t image_index,
                                       const FrameRequest& request) {
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  requireVulkan(vkBeginCommandBuffer(command_buffer, &begin_info),
                "Begin Vulkan frame command buffer");

  VkImageMemoryBarrier2 to_color{};
  to_color.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  to_color.srcStageMask = image_initialized_[image_index]
                              ? VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
                              : VK_PIPELINE_STAGE_2_NONE;
  to_color.srcAccessMask = 0;
  to_color.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_color.oldLayout = image_initialized_[image_index]
                           ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                           : VK_IMAGE_LAYOUT_UNDEFINED;
  to_color.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  to_color.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  to_color.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  to_color.image = swapchain_images_[image_index];
  to_color.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  to_color.subresourceRange.levelCount = 1;
  to_color.subresourceRange.layerCount = 1;
  DepthAttachment& depth = *depth_attachments_[image_index];
  VkImageMemoryBarrier2 to_depth{};
  to_depth.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  to_depth.srcStageMask = depth.initialized()
                              ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT
                              : VK_PIPELINE_STAGE_2_NONE;
  to_depth.srcAccessMask =
      depth.initialized() ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0;
  to_depth.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                          VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
  to_depth.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  to_depth.oldLayout = depth.initialized()
                           ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
                           : VK_IMAGE_LAYOUT_UNDEFINED;
  to_depth.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  to_depth.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  to_depth.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  to_depth.image = depth.image();
  to_depth.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  to_depth.subresourceRange.levelCount = 1;
  to_depth.subresourceRange.layerCount = 1;
  const std::array<VkImageMemoryBarrier2, 2> attachment_barriers = {to_color,
                                                                    to_depth};
  VkDependencyInfo dependency{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.imageMemoryBarrierCount =
      static_cast<std::uint32_t>(attachment_barriers.size());
  dependency.pImageMemoryBarriers = attachment_barriers.data();
  vkCmdPipelineBarrier2(command_buffer, &dependency);

  const VkClearValue clear = {{{0.02F, 0.025F, 0.04F, 1.0F}}};
  VkRenderingAttachmentInfo color_attachment{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.imageView = swapchain_views_[image_index];
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.clearValue = clear;
  VkRenderingAttachmentInfo depth_attachment{};
  depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  depth_attachment.imageView = depth.view();
  depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.clearValue.depthStencil = {1.0F, 0};
  VkRenderingInfo rendering_info{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea.extent = swapchain_extent_;
  rendering_info.layerCount = 1;
  rendering_info.colorAttachmentCount = 1;
  rendering_info.pColorAttachments = &color_attachment;
  rendering_info.pDepthAttachment = &depth_attachment;
  vkCmdBeginRendering(command_buffer, &rendering_info);
  const VkViewport viewport{0.0F,
                            0.0F,
                            static_cast<float>(swapchain_extent_.width),
                            static_cast<float>(swapchain_extent_.height),
                            0.0F,
                            1.0F};
  const VkRect2D scissor{{0, 0}, swapchain_extent_};
  vkCmdSetViewport(command_buffer, 0, 1, &viewport);
  vkCmdSetScissor(command_buffer, 0, 1, &scissor);
  if (pipeline_ != nullptr) {
    pipeline_->bindSceneState(command_buffer, request.camera,
                              request.spot_light, request.point_light_enabled);
    if (world_mesh_) world_mesh_->bindAndDraw(command_buffer);
    chair_mesh_->bindAndDraw(command_buffer);
  }
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
  vkCmdEndRendering(command_buffer);

  VkImageMemoryBarrier2 to_present{};
  to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  to_present.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_present.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_present.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
  to_present.dstAccessMask = 0;
  to_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  to_present.image = swapchain_images_[image_index];
  to_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  to_present.subresourceRange.levelCount = 1;
  to_present.subresourceRange.layerCount = 1;
  dependency.imageMemoryBarrierCount = 1;
  dependency.pImageMemoryBarriers = &to_present;
  vkCmdPipelineBarrier2(command_buffer, &dependency);
  requireVulkan(vkEndCommandBuffer(command_buffer),
                "End Vulkan frame command buffer");
}

FrameOutcome EditorRenderer::Impl::renderFrame(const FrameRequest& request) {
  recreate_requested_ = request.framebuffer_resized || recreate_requested_;
  if (recreate_requested_) {
    recreateSwapchain(request.framebuffer);
    return FrameOutcome::Recovered;
  }

  FrameSlot& frame = frames_[current_frame_];
  requireVulkan(
      vkWaitForFences(context_.device(), 1, &frame.completion, VK_TRUE,
                      std::numeric_limits<std::uint64_t>::max()),
      "Wait for frame slot completion before reuse");

  std::uint32_t image_index = 0;
  const VkResult acquire = vkAcquireNextImageKHR(
      context_.device(), swapchain_, std::numeric_limits<std::uint64_t>::max(),
      frame.image_available, VK_NULL_HANDLE, &image_index);
  if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
    recreate_requested_ = true;
    return FrameOutcome::Recovered;
  }
  if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
    requireVulkan(acquire, "Acquire Vulkan swapchain image");
  }
  const bool suboptimal = acquire == VK_SUBOPTIMAL_KHR;
  if (image_fences_[image_index] != VK_NULL_HANDLE) {
    requireVulkan(
        vkWaitForFences(context_.device(), 1, &image_fences_[image_index],
                        VK_TRUE, std::numeric_limits<std::uint64_t>::max()),
        "Wait for prior use of Vulkan swapchain image");
  }

  requireVulkan(vkResetCommandPool(context_.device(), frame.command_pool, 0),
                "Reset per-frame Vulkan command pool");
  recordFrame(frame.command_buffer, image_index, request);
  requireVulkan(vkResetFences(context_.device(), 1, &frame.completion),
                "Reset Vulkan frame completion fence");

  VkSemaphoreSubmitInfo wait_info{};
  wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  wait_info.semaphore = frame.image_available;
  wait_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkCommandBufferSubmitInfo command_info{};
  command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  command_info.commandBuffer = frame.command_buffer;
  VkSemaphoreSubmitInfo signal_info{};
  signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  signal_info.semaphore = render_finished_[image_index];
  signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
  VkSubmitInfo2 submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit_info.waitSemaphoreInfoCount = 1;
  submit_info.pWaitSemaphoreInfos = &wait_info;
  submit_info.commandBufferInfoCount = 1;
  submit_info.pCommandBufferInfos = &command_info;
  submit_info.signalSemaphoreInfoCount = 1;
  submit_info.pSignalSemaphoreInfos = &signal_info;
  requireVulkan(vkQueueSubmit2(context_.graphicsQueue(), 1, &submit_info,
                               frame.completion),
                "Submit Vulkan frame with Synchronization 2");
  image_fences_[image_index] = frame.completion;
  image_initialized_[image_index] = true;
  depth_attachments_[image_index]->markInitialized();

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_finished_[image_index];
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain_;
  present_info.pImageIndices = &image_index;
  const VkResult present =
      vkQueuePresentKHR(context_.presentQueue(), &present_info);
  if (present != VK_SUCCESS && present != VK_SUBOPTIMAL_KHR &&
      present != VK_ERROR_OUT_OF_DATE_KHR) {
    requireVulkan(present, "Present Vulkan swapchain image");
  }
  current_frame_ = (current_frame_ + 1) % frames_in_flight;

  if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR ||
      suboptimal) {
    recreate_requested_ = true;
    return FrameOutcome::Recovered;
  }
  return FrameOutcome::Rendered;
}
