#include "core/render/vulkan_context.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/platform/glfw_vulkan_bridge.hpp"
#include "core/platform/window.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "core/testing/test_controls.hpp"

namespace {
constexpr const char* validation_layer = "VK_LAYER_KHRONOS_validation";

VKAPI_ATTR VkBool32 VKAPI_CALL validationCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
  auto decoded_severity = ValidationSeverity::Info;
  if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
    decoded_severity = ValidationSeverity::Error;
  } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) !=
             0) {
    decoded_severity = ValidationSeverity::Warning;
  } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) !=
             0) {
    decoded_severity = ValidationSeverity::Verbose;
  }

  const unsigned category_count =
      ((types & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) != 0 ? 1U : 0U) +
      ((types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0 ? 1U
                                                                    : 0U) +
      ((types & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0 ? 1U
                                                                     : 0U);
  ValidationCategory category = ValidationCategory::General;
  if (category_count > 1U) {
    category = ValidationCategory::Multiple;
  } else if ((types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0) {
    category = ValidationCategory::Validation;
  } else if ((types & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0) {
    category = ValidationCategory::Performance;
  }

  if (user_data != nullptr) {
    static_cast<ValidationDiagnostics*>(user_data)->record(
        decoded_severity, category,
        callback_data != nullptr && callback_data->pMessage != nullptr
            ? callback_data->pMessage
            : "unknown message");
  }
  return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo(
    ValidationDiagnostics& diagnostics) {
  VkDebugUtilsMessengerCreateInfoEXT info{
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
  info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  info.pfnUserCallback = validationCallback;
  info.pUserData = &diagnostics;
  return info;
}

bool hasLayer(const char* name) {
  std::uint32_t count = 0;
  requireVulkan(vkEnumerateInstanceLayerProperties(&count, nullptr),
                "Enumerate Vulkan instance layers");
  std::vector<VkLayerProperties> layers(count);
  requireVulkan(vkEnumerateInstanceLayerProperties(&count, layers.data()),
                "Enumerate Vulkan instance layers");
  return std::any_of(layers.begin(), layers.end(), [name](const auto& layer) {
    return std::strcmp(layer.layerName, name) == 0;
  });
}

bool hasInstanceExtension(const char* name) {
  std::uint32_t count = 0;
  requireVulkan(
      vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr),
      "Enumerate Vulkan instance extensions");
  std::vector<VkExtensionProperties> extensions(count);
  requireVulkan(vkEnumerateInstanceExtensionProperties(nullptr, &count,
                                                       extensions.data()),
                "Enumerate Vulkan instance extensions");
  return std::any_of(extensions.begin(), extensions.end(),
                     [name](const auto& extension) {
                       return std::strcmp(extension.extensionName, name) == 0;
                     });
}

bool hasDeviceExtension(VkPhysicalDevice device, const char* name) {
  std::uint32_t count = 0;
  requireVulkan(
      vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr),
      "Enumerate Vulkan device extensions");
  std::vector<VkExtensionProperties> extensions(count);
  requireVulkan(vkEnumerateDeviceExtensionProperties(device, nullptr, &count,
                                                     extensions.data()),
                "Enumerate Vulkan device extensions");
  return std::any_of(extensions.begin(), extensions.end(),
                     [name](const auto& extension) {
                       return std::strcmp(extension.extensionName, name) == 0;
                     });
}

struct DeviceCandidate {
  VkPhysicalDevice device{VK_NULL_HANDLE};
  QueueFamilySelection queues{};
  bool discrete{};
};

std::optional<DeviceCandidate> inspectDevice(VkPhysicalDevice device,
                                             VkSurfaceKHR surface,
                                             std::string& failure) {
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(device, &properties);
  const std::string name = properties.deviceName;
  if (properties.apiVersion < VK_API_VERSION_1_3) {
    failure = name + ": Vulkan 1.3 is unavailable";
    return std::nullopt;
  }
  if (!hasDeviceExtension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
    failure = name + ": VK_KHR_swapchain is unavailable";
    return std::nullopt;
  }

  VkPhysicalDeviceVulkan13Features features13{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  VkPhysicalDeviceFeatures2 features2{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  features2.pNext = &features13;
  vkGetPhysicalDeviceFeatures2(device, &features2);
  if (features13.dynamicRendering != VK_TRUE) {
    failure = name + ": Dynamic Rendering is unavailable";
    return std::nullopt;
  }
  if (features13.synchronization2 != VK_TRUE) {
    failure = name + ": Synchronization 2 is unavailable";
    return std::nullopt;
  }

  std::uint32_t queue_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_count, nullptr);
  std::vector<VkQueueFamilyProperties> properties_list(queue_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_count,
                                           properties_list.data());
  std::vector<QueueFamilyCandidate> queue_candidates(queue_count);
  for (std::uint32_t index = 0; index < queue_count; ++index) {
    VkBool32 supports_present = VK_FALSE;
    requireVulkan(vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface,
                                                       &supports_present),
                  "Query Vulkan presentation queue support");
    queue_candidates[index] = {
        (properties_list[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0,
        supports_present == VK_TRUE};
  }
  const auto queues = chooseQueueFamilies(queue_candidates);
  if (!queues) {
    failure = name + ": no graphics/presentation queue configuration";
    return std::nullopt;
  }

  std::uint32_t format_count = 0;
  std::uint32_t present_mode_count = 0;
  requireVulkan(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface,
                                                     &format_count, nullptr),
                "Query Vulkan surface formats");
  requireVulkan(vkGetPhysicalDeviceSurfacePresentModesKHR(
                    device, surface, &present_mode_count, nullptr),
                "Query Vulkan presentation modes");
  if (format_count == 0 || present_mode_count == 0) {
    failure = name + ": swapchain formats or presentation modes unavailable";
    return std::nullopt;
  }

  return DeviceCandidate{
      device, *queues,
      properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU};
}
}  // namespace

VulkanContext::VulkanContext(const Window& window,
                             ValidationDiagnostics& diagnostics)
    : window_(window), diagnostics_(diagnostics) {
  try {
    createInstance();
    createDebugMessenger();
    if (forcedVulkanFailureAt("instance")) {
      throw std::runtime_error("Forced failure after Vulkan instance creation");
    }
    surface_ = GlfwVulkanBridge::createSurface(instance_, window_);
    if (forcedVulkanFailureAt("surface")) {
      throw std::runtime_error("Forced failure after Vulkan surface creation");
    }
    selectPhysicalDevice();
    createDevice();
    if (forcedVulkanFailureAt("device")) {
      throw std::runtime_error("Forced failure after Vulkan device creation");
    }
  } catch (...) {
    cleanup();
    throw;
  }
}

VulkanContext::~VulkanContext() { cleanup(); }

void VulkanContext::createInstance() {
  std::vector<const char*> extensions =
      GlfwVulkanBridge::requiredInstanceExtensions();

#if defined(NEAR_LAUGH_ENABLE_VULKAN_VALIDATION)
  if (hasLayer(validation_layer) &&
      hasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    validation_enabled_ = true;
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  } else {
    std::cerr << "Vulkan validation requested but the Khronos layer or "
                 "VK_EXT_debug_utils is unavailable; continuing without "
                 "validation\n";
  }
#endif

  VkApplicationInfo application_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  application_info.pApplicationName = "near-laugh fps";
  application_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  application_info.pEngineName = "near-laugh";
  application_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  application_info.apiVersion = VK_API_VERSION_1_3;

  VkDebugUtilsMessengerCreateInfoEXT debug_info =
      debugMessengerInfo(diagnostics_);
  VkInstanceCreateInfo create_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  create_info.pApplicationInfo = &application_info;
  create_info.enabledExtensionCount =
      static_cast<std::uint32_t>(extensions.size());
  create_info.ppEnabledExtensionNames = extensions.data();
  if (validation_enabled_) {
    create_info.enabledLayerCount = 1;
    create_info.ppEnabledLayerNames = &validation_layer;
    create_info.pNext = &debug_info;
  }
  requireVulkan(vkCreateInstance(&create_info, nullptr, &instance_),
                "Create Vulkan 1.3 instance");
  std::cout << "Vulkan validation: "
            << (validation_enabled_ ? "enabled" : "disabled") << '\n';
}

void VulkanContext::createDebugMessenger() {
  if (!validation_enabled_) {
    return;
  }
  const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
  if (create == nullptr) {
    throw std::runtime_error(
        "VK_EXT_debug_utils was enabled but its create function is missing");
  }
  VkDebugUtilsMessengerCreateInfoEXT info = debugMessengerInfo(diagnostics_);
  requireVulkan(create(instance_, &info, nullptr, &debug_messenger_),
                "Create Vulkan debug messenger");
}

void VulkanContext::selectPhysicalDevice() {
  std::uint32_t count = 0;
  requireVulkan(vkEnumeratePhysicalDevices(instance_, &count, nullptr),
                "Enumerate Vulkan physical devices");
  if (count == 0) {
    throw std::runtime_error("No Vulkan physical devices were found");
  }
  std::vector<VkPhysicalDevice> devices(count);
  requireVulkan(vkEnumeratePhysicalDevices(instance_, &count, devices.data()),
                "Enumerate Vulkan physical devices");

  std::optional<DeviceCandidate> selected;
  std::string failures;
  for (VkPhysicalDevice device : devices) {
    std::string failure;
    const auto candidate = inspectDevice(device, surface_, failure);
    if (!candidate) {
      failures += "\n - " + failure;
      continue;
    }
    if (!selected || (!selected->discrete && candidate->discrete)) {
      selected = candidate;
    }
  }
  if (!selected) {
    throw std::runtime_error(
        "No physical device satisfies Vulkan 1.3, Dynamic Rendering, "
        "Synchronization 2, swapchain, graphics, and presentation "
        "requirements:" +
        failures);
  }

  physical_device_ = selected->device;
  queue_families_ = selected->queues;
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physical_device_, &properties);
  std::cout << "Selected Vulkan GPU: " << properties.deviceName
            << " (graphics queue " << queue_families_.graphics
            << ", present queue " << queue_families_.present << ")\n";
}

void VulkanContext::createDevice() {
  const std::set<std::uint32_t> unique_families = {queue_families_.graphics,
                                                   queue_families_.present};
  const float priority = 1.0F;
  std::vector<VkDeviceQueueCreateInfo> queues;
  queues.reserve(unique_families.size());
  for (const std::uint32_t family : unique_families) {
    VkDeviceQueueCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    info.queueFamilyIndex = family;
    info.queueCount = 1;
    info.pQueuePriorities = &priority;
    queues.push_back(info);
  }

  VkPhysicalDeviceVulkan13Features features13{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  features13.dynamicRendering = VK_TRUE;
  features13.synchronization2 = VK_TRUE;
  const char* extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
  VkDeviceCreateInfo create_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  create_info.pNext = &features13;
  create_info.queueCreateInfoCount = static_cast<std::uint32_t>(queues.size());
  create_info.pQueueCreateInfos = queues.data();
  create_info.enabledExtensionCount = 1;
  create_info.ppEnabledExtensionNames = &extension;
  requireVulkan(
      vkCreateDevice(physical_device_, &create_info, nullptr, &device_),
      "Create Vulkan logical device");
  vkGetDeviceQueue(device_, queue_families_.graphics, 0, &graphics_queue_);
  vkGetDeviceQueue(device_, queue_families_.present, 0, &present_queue_);
}

void VulkanContext::cleanup() noexcept {
  if (device_ != VK_NULL_HANDLE) {
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
  }
  if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
  }
  if (debug_messenger_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
    const auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
    if (destroy != nullptr) {
      destroy(instance_, debug_messenger_, nullptr);
    }
    debug_messenger_ = VK_NULL_HANDLE;
  }
  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
  }
}
