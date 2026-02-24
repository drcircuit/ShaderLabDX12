// ---------------------------------------------------------------------------
// VulkanDevice.cpp  –  Vulkan graphics device (Linux / macOS)
//
// Implementation notes:
//   - Surface creation delegates to SDL2 (SDL_Vulkan_CreateSurface) when
//     SHADERLAB_WINDOW_SDL2 is defined.
//   - On macOS, MoltenVK is loaded as the Vulkan ICD; no Metal-specific code
//     is required in this file.
//   - Validation layers are enabled when enableValidation == true and the
//     VK_LAYER_KHRONOS_validation layer is present.
// ---------------------------------------------------------------------------

#include "ShaderLab/Graphics/Vulkan/VulkanDevice.h"

#ifdef SHADERLAB_WINDOW_SDL2
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#endif

#include <cstring>
#include <stdexcept>
#include <vector>

namespace ShaderLab {

// ---------------------------------------------------------------------------
// Debug messenger callback
// ---------------------------------------------------------------------------
#ifndef NDEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                       /*user*/)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        // Use fprintf so that it works even before a console is fully set up.
        std::fprintf(stderr, "[Vulkan] %s\n", data->pMessage);
    }
    return VK_FALSE;
}
#endif

// ---------------------------------------------------------------------------
VulkanDevice::VulkanDevice()  = default;
VulkanDevice::~VulkanDevice() { Shutdown(); }

// ---------------------------------------------------------------------------
std::vector<VulkanDevice::AdapterInfo> VulkanDevice::GetAvailableAdapters() {
    std::vector<AdapterInfo> result;

    VkInstance tempInstance = VK_NULL_HANDLE;
    VkApplicationInfo appInfo{};
    appInfo.sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &appInfo;

    if (vkCreateInstance(&ci, nullptr, &tempInstance) != VK_SUCCESS) {
        return result;
    }

    uint32_t count = 0;
    vkEnumeratePhysicalDevices(tempInstance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(tempInstance, &count, devices.data());

    for (uint32_t i = 0; i < count; ++i) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(devices[i], &props);

        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(devices[i], &memProps);

        size_t localMemory = 0;
        for (uint32_t h = 0; h < memProps.memoryHeapCount; ++h) {
            if (memProps.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                localMemory += memProps.memoryHeaps[h].size;
            }
        }

        AdapterInfo info;
        info.name        = props.deviceName;
        info.videoMemory = localMemory;
        info.index       = i;
        result.push_back(info);
    }

    vkDestroyInstance(tempInstance, nullptr);
    return result;
}

// ---------------------------------------------------------------------------
bool VulkanDevice::Initialize(NativeWindowHandle window,
                               bool               enableValidation,
                               int                adapterIndex) {
    m_lastInitFailureCode = InitFailureCode::None;

    if (!CreateInstance(enableValidation)) {
        m_lastInitFailureCode = InitFailureCode::InstanceCreateFailed;
        return false;
    }

    // Create window surface via SDL2 Vulkan integration.
#ifdef SHADERLAB_WINDOW_SDL2
    SDL_Window* sdlWindow = reinterpret_cast<SDL_Window*>(window);
    if (!SDL_Vulkan_CreateSurface(sdlWindow, m_instance, &m_surface)) {
        std::fprintf(stderr, "[VulkanDevice] SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
        m_lastInitFailureCode = InitFailureCode::SurfaceCreateFailed;
        return false;
    }
#else
    // For non-SDL2 platforms, surface creation should be provided by the
    // window backend (e.g. vkCreateXcbSurfaceKHR on bare Linux).
    (void)window;
#endif

    if (!PickPhysicalDevice(adapterIndex)) {
        m_lastInitFailureCode = InitFailureCode::PhysicalDeviceSelectionFailed;
        return false;
    }

    if (!CreateLogicalDevice()) {
        m_lastInitFailureCode = InitFailureCode::LogicalDeviceCreateFailed;
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
void VulkanDevice::Shutdown() {
    if (m_device) {
        vkDeviceWaitIdle(m_device);
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    if (m_surface) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
#ifndef NDEBUG
    if (m_debugMessenger) {
        auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (fn) fn(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }
#endif
    if (m_instance) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

// ---------------------------------------------------------------------------
VulkanDevice::MemoryInfo VulkanDevice::GetVideoMemoryInfo() const {
    MemoryInfo info{};
    if (!m_physicalDevice) return info;

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            info.budget += memProps.memoryHeaps[i].size;
        }
    }
    // Accurate current usage requires VK_EXT_memory_budget; omit for now.
    return info;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
bool VulkanDevice::CreateInstance(bool enableValidation) {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "ShaderLab";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "ShaderLabEngine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    // Extensions required by SDL2 + optional debug utils
    std::vector<const char*> extensions;
#ifdef SHADERLAB_WINDOW_SDL2
    uint32_t sdlExtCount = 0;
    SDL_Vulkan_GetInstanceExtensions(nullptr, &sdlExtCount, nullptr);
    extensions.resize(sdlExtCount);
    SDL_Vulkan_GetInstanceExtensions(nullptr, &sdlExtCount, extensions.data());
#endif
    if (enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    const char* validationLayer = "VK_LAYER_KHRONOS_validation";
    std::vector<const char*> layers;
    if (enableValidation) {
        layers.push_back(validationLayer);
    }

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();
    ci.enabledLayerCount       = static_cast<uint32_t>(layers.size());
    ci.ppEnabledLayerNames     = layers.data();

    if (vkCreateInstance(&ci, nullptr, &m_instance) != VK_SUCCESS) {
        return false;
    }

#ifndef NDEBUG
    if (enableValidation) {
        VkDebugUtilsMessengerCreateInfoEXT dbgCi{};
        dbgCi.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbgCi.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbgCi.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        dbgCi.pfnUserCallback = DebugCallback;

        auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
        if (fn) fn(m_instance, &dbgCi, nullptr, &m_debugMessenger);
    }
#endif

    return true;
}

bool VulkanDevice::PickPhysicalDevice(int preferredIndex) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) return false;

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    // Use the explicitly requested adapter if valid.
    if (preferredIndex >= 0 && static_cast<uint32_t>(preferredIndex) < count) {
        m_physicalDevice = devices[preferredIndex];
        m_adapterIndex   = preferredIndex;
    } else {
        // Prefer a discrete GPU, fall back to any device.
        for (uint32_t i = 0; i < count; ++i) {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(devices[i], &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                m_physicalDevice = devices[i];
                m_adapterIndex   = static_cast<int>(i);
                break;
            }
        }
        if (!m_physicalDevice) {
            m_physicalDevice = devices[0];
            m_adapterIndex   = 0;
        }
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    m_adapterName = props.deviceName;

    // Find a graphics queue family that supports presenting to our surface.
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, families.data());

    for (uint32_t i = 0; i < queueCount; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 presentSupport = VK_FALSE;
            if (m_surface) {
                vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &presentSupport);
            }
            if (!m_surface || presentSupport) {
                m_graphicsFamily = i;
                return true;
            }
        }
    }
    return false;
}

bool VulkanDevice::CreateLogicalDevice() {
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCi{};
    queueCi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCi.queueFamilyIndex = m_graphicsFamily;
    queueCi.queueCount       = 1;
    queueCi.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount    = 1;
    ci.pQueueCreateInfos       = &queueCi;
    ci.enabledExtensionCount   = 1;
    ci.ppEnabledExtensionNames = deviceExtensions;
    ci.pEnabledFeatures        = &features;

    if (vkCreateDevice(m_physicalDevice, &ci, nullptr, &m_device) != VK_SUCCESS) {
        return false;
    }

    vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
    return true;
}

} // namespace ShaderLab
