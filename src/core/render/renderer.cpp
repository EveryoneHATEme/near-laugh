#include "renderer.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "core/platform/window.hpp"
#include "core/render/graphics_pipeline.hpp"
#include "core/render/vulkan_utils.hpp"

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
}  // namespace

Renderer::Renderer(Window& window) : window_(window), context_(window) {
  try {
    createSwapchain();
    pipeline_ = std::make_unique<GraphicsPipeline>(
        context_.device(), context_.physicalDevice(), swapchain_format_,
        std::filesystem::path("resources/shaders/triangle_vertex.spv"),
        std::filesystem::path("resources/shaders/triangle_fragment.spv"));
    createFrameSlots();
  } catch (...) {
    cleanupFrameSlots();
    pipeline_.reset();
    cleanupSwapchain();
    throw;
  }
}

Renderer::~Renderer() {
  if (context_.device() != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(context_.device());
  }
  cleanupFrameSlots();
  pipeline_.reset();
  cleanupSwapchain();
}

bool Renderer::validationEnabled() const noexcept {
  return context_.validationEnabled();
}

void Renderer::requestSwapchainRecreation() noexcept {
  recreate_requested_ = true;
}

void Renderer::createSwapchain() {
  const SwapchainSupport support =
      querySwapchainSupport(context_.physicalDevice(), context_.surface());
  if (support.formats.empty() || support.present_modes.empty()) {
    throw std::runtime_error(
        "Swapchain creation failed: surface formats or presentation modes are "
        "empty");
  }
  const VkSurfaceFormatKHR surface_format =
      chooseSurfaceFormat(support.formats);
  const FramebufferExtent framebuffer = window_.framebufferExtent();
  const VkExtent2D extent = chooseSwapchainExtent(
      support.capabilities, {framebuffer.width, framebuffer.height});
  std::uint32_t image_count = support.capabilities.minImageCount + 1;
  if (support.capabilities.maxImageCount != 0) {
    image_count = std::min(image_count, support.capabilities.maxImageCount);
  }

  const QueueFamilySelection families = context_.queueFamilies();
  const std::array<std::uint32_t, 2> family_indices = {families.graphics,
                                                       families.present};
  VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
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
  info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
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
  try {
    for (std::size_t index = 0; index < swapchain_images_.size(); ++index) {
      VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
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

void Renderer::cleanupSwapchain() noexcept {
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

bool Renderer::waitForRenderableExtent() {
  FramebufferExtent extent = window_.framebufferExtent();
  while (extent.isZero() && !window_.shouldClose()) {
    window_.waitEvents();
    extent = window_.framebufferExtent();
  }
  return !extent.isZero() && !window_.shouldClose();
}

void Renderer::recreateSwapchain() {
  if (!waitForRenderableExtent()) {
    return;
  }
  requireVulkan(vkDeviceWaitIdle(context_.device()),
                "Wait for Vulkan device before swapchain recreation");
  const VkFormat previous_format = swapchain_format_;
  cleanupSwapchain();
  createSwapchain();
  if (pipeline_ != nullptr && previous_format != swapchain_format_) {
    pipeline_ = std::make_unique<GraphicsPipeline>(
        context_.device(), context_.physicalDevice(), swapchain_format_,
        std::filesystem::path("resources/shaders/triangle_vertex.spv"),
        std::filesystem::path("resources/shaders/triangle_fragment.spv"));
  }
  recreate_requested_ = false;
}

void Renderer::createFrameSlots() {
  try {
    for (FrameSlot& frame : frames_) {
      VkCommandPoolCreateInfo pool_info{
          VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
      pool_info.queueFamilyIndex = context_.queueFamilies().graphics;
      pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
      requireVulkan(vkCreateCommandPool(context_.device(), &pool_info, nullptr,
                                        &frame.command_pool),
                    "Create per-frame Vulkan command pool");

      VkCommandBufferAllocateInfo allocate_info{
          VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
      allocate_info.commandPool = frame.command_pool;
      allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocate_info.commandBufferCount = 1;
      requireVulkan(vkAllocateCommandBuffers(context_.device(), &allocate_info,
                                             &frame.command_buffer),
                    "Allocate per-frame Vulkan command buffer");
      VkSemaphoreCreateInfo semaphore_info{
          VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
      requireVulkan(vkCreateSemaphore(context_.device(), &semaphore_info,
                                      nullptr, &frame.image_available),
                    "Create image-available semaphore");
      requireVulkan(vkCreateSemaphore(context_.device(), &semaphore_info,
                                      nullptr, &frame.render_finished),
                    "Create render-finished semaphore");
      VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
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

void Renderer::cleanupFrameSlots() noexcept {
  for (FrameSlot& frame : frames_) {
    if (frame.completion != VK_NULL_HANDLE) {
      vkDestroyFence(context_.device(), frame.completion, nullptr);
      frame.completion = VK_NULL_HANDLE;
    }
    if (frame.render_finished != VK_NULL_HANDLE) {
      vkDestroySemaphore(context_.device(), frame.render_finished, nullptr);
      frame.render_finished = VK_NULL_HANDLE;
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

void Renderer::recordFrame(VkCommandBuffer command_buffer,
                           std::uint32_t image_index) {
  VkCommandBufferBeginInfo begin_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  requireVulkan(vkBeginCommandBuffer(command_buffer, &begin_info),
                "Begin Vulkan frame command buffer");

  VkImageMemoryBarrier2 to_color{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
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
  VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependency.imageMemoryBarrierCount = 1;
  dependency.pImageMemoryBarriers = &to_color;
  vkCmdPipelineBarrier2(command_buffer, &dependency);

  const VkClearValue clear = {{{0.02F, 0.025F, 0.04F, 1.0F}}};
  VkRenderingAttachmentInfo color_attachment{
      VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  color_attachment.imageView = swapchain_views_[image_index];
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.clearValue = clear;
  VkRenderingInfo rendering_info{VK_STRUCTURE_TYPE_RENDERING_INFO};
  rendering_info.renderArea.extent = swapchain_extent_;
  rendering_info.layerCount = 1;
  rendering_info.colorAttachmentCount = 1;
  rendering_info.pColorAttachments = &color_attachment;
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
  pipeline_->bindAndDraw(command_buffer);
  vkCmdEndRendering(command_buffer);

  VkImageMemoryBarrier2 to_present{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
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
  dependency.pImageMemoryBarriers = &to_present;
  vkCmdPipelineBarrier2(command_buffer, &dependency);
  requireVulkan(vkEndCommandBuffer(command_buffer),
                "End Vulkan frame command buffer");
}

bool Renderer::renderFrame() {
  recreate_requested_ =
      window_.consumeFramebufferResize() || recreate_requested_;
  if (recreate_requested_) {
    recreateSwapchain();
    if (window_.shouldClose()) {
      return false;
    }
  }
  if (window_.framebufferExtent().isZero()) {
    if (!waitForRenderableExtent()) {
      return false;
    }
    recreateSwapchain();
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
    recreateSwapchain();
    return false;
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
  recordFrame(frame.command_buffer, image_index);
  requireVulkan(vkResetFences(context_.device(), 1, &frame.completion),
                "Reset Vulkan frame completion fence");

  VkSemaphoreSubmitInfo wait_info{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
  wait_info.semaphore = frame.image_available;
  wait_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkCommandBufferSubmitInfo command_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
  command_info.commandBuffer = frame.command_buffer;
  VkSemaphoreSubmitInfo signal_info{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
  signal_info.semaphore = frame.render_finished;
  signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
  VkSubmitInfo2 submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
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

  VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &frame.render_finished;
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
      suboptimal || window_.consumeFramebufferResize()) {
    recreate_requested_ = true;
  }
  return true;
}
