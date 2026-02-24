#pragma once

// ---------------------------------------------------------------------------
// VulkanDevice
//
// Vulkan implementation of the graphics device layer.
// Replaces D3D12 Device for Linux and macOS builds.
//
// Dependencies (install via package manager or vcpkg):
//   Linux  : apt install libvulkan-dev vulkan-validationlayers
//   macOS  : brew install molten-vk vulkan-validationlayers
//            MoltenVK translates Vulkan API calls to Metal automatically.
// ---------------------------------------------------------------------------

#include "ShaderLab/Platform/Platform.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <cstdint>

namespace ShaderLab {

class VulkanDevice {
public:
    enum class InitFailureCode {
        None = 0,
        InstanceCreateFailed,
        SurfaceCreateFailed,
        PhysicalDeviceSelectionFailed,
        LogicalDeviceCreateFailed,
    };

    VulkanDevice();
    ~VulkanDevice();

    struct AdapterInfo {
        std::string  name;
        size_t       videoMemory = 0;
        uint32_t     index       = 0;
    };

    static std::vector<AdapterInfo> GetAvailableAdapters();

    // window is a NativeWindowHandle (SDL_Window* on SDL2 builds).
    bool Initialize(NativeWindowHandle window,
                    bool enableValidation = false,
                    int adapterIndex     = -1);
    void Shutdown();

    VkInstance       GetInstance()       const { return m_instance; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkDevice         GetDevice()         const { return m_device; }
    VkQueue          GetGraphicsQueue()  const { return m_graphicsQueue; }
    VkSurfaceKHR     GetSurface()        const { return m_surface; }
    uint32_t         GetGraphicsFamily() const { return m_graphicsFamily; }

    struct MemoryInfo {
        uint64_t usage  = 0;
        uint64_t budget = 0;
    };
    MemoryInfo GetVideoMemoryInfo() const;

    bool IsValid()          const { return m_device != VK_NULL_HANDLE; }
    std::string GetAdapterName()  const { return m_adapterName; }
    int         GetAdapterIndex() const { return m_adapterIndex; }
    InitFailureCode GetLastInitFailureCode() const { return m_lastInitFailureCode; }

private:
    bool CreateInstance(bool enableValidation);
    bool PickPhysicalDevice(int preferredIndex);
    bool CreateLogicalDevice();

    VkInstance       m_instance       = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice         m_device         = VK_NULL_HANDLE;
    VkSurfaceKHR     m_surface        = VK_NULL_HANDLE;
    VkQueue          m_graphicsQueue  = VK_NULL_HANDLE;
    uint32_t         m_graphicsFamily = 0;

    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

    std::string     m_adapterName;
    int             m_adapterIndex   = -1;
    InitFailureCode m_lastInitFailureCode = InitFailureCode::None;
};

} // namespace ShaderLab
