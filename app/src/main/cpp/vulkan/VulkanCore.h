#ifndef VULKAN_CORE_H
#define VULKAN_CORE_H

#include <vulkan/vulkan.h>
#include <android/native_window_jni.h>
#include <memory>
#include <vector>

namespace fluidsim {

class VulkanInstance;
class VulkanSurface;
class VulkanDevice;
class VulkanSwapchain;
class VulkanCommand;

class VulkanCore {
public:
    VulkanCore();
    ~VulkanCore();

    bool initialize(ANativeWindow* window, int width, int height);
    void cleanup();

    VkInstance getInstance() const;
    VkPhysicalDevice getPhysicalDevice() const { return mPhysicalDevice; }
    VkDevice getDevice() const;
    VkSurfaceKHR getSurface() const;
    VkQueue getGraphicsQueue() const;
    VkQueue getPresentQueue() const;
    uint32_t getGraphicsQueueFamilyIndex() const;
    uint32_t getPresentQueueFamilyIndex() const;

    bool createCommandPool(uint32_t queueFamilyIndex);
    void destroyCommandPool();

    bool allocateCommandBuffers(uint32_t count);
    void freeCommandBuffers();

    bool beginSingleTimeCommands();
    void endSingleTimeCommands();

    uint32_t acquireNextImage(VkSemaphore semaphore);
    void presentFrame(VkSemaphore semaphore, uint32_t imageIndex);

    bool isDeviceSuitable(VkPhysicalDevice device);
    bool findQueueFamilies(VkPhysicalDevice device);

    std::vector<const char*> getRequiredInstanceExtensions();
    std::vector<const char*> getRequiredDeviceExtensions();

    VulkanInstance* getInstanceComponent() { return mInstance.get(); }
    VulkanSurface* getSurfaceComponent() { return mSurface.get(); }
    VulkanDevice* getDeviceComponent() { return mDevice.get(); }
    VulkanSwapchain* getSwapchainComponent() { return mSwapchain.get(); }
    VulkanCommand* getCommandComponent() { return mCommand.get(); }

private:
    std::unique_ptr<VulkanInstance> mInstance;
    std::unique_ptr<VulkanSurface> mSurface;
    std::unique_ptr<VulkanDevice> mDevice;
    std::unique_ptr<VulkanSwapchain> mSwapchain;
    std::unique_ptr<VulkanCommand> mCommand;

    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    int mWidth = 0;
    int mHeight = 0;

    // Helpers
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
};

} // namespace fluidsim

#endif // VULKAN_CORE_H
