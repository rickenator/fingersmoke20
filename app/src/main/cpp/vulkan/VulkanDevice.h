#ifndef VULKAN_DEVICE_H
#define VULKAN_DEVICE_H

#include <vulkan/vulkan.h>
#include <vector>

namespace fluidsim {

class VulkanDevice {
public:
    VulkanDevice();
    ~VulkanDevice();

    bool createDevice(VkPhysicalDevice physicalDevice, uint32_t graphicsQueueFamilyIndex, uint32_t presentQueueFamilyIndex);
    void destroyDevice();

    VkDevice getDevice() const { return mDevice; }
    VkQueue getGraphicsQueue() const { return mGraphicsQueue; }
    VkQueue getPresentQueue() const { return mPresentQueue; }
    uint32_t getGraphicsQueueFamilyIndex() const { return mGraphicsQueueFamilyIndex; }
    uint32_t getPresentQueueFamilyIndex() const { return mPresentQueueFamilyIndex; }

private:
    VkDevice mDevice = VK_NULL_HANDLE;
    VkQueue mGraphicsQueue = VK_NULL_HANDLE;
    VkQueue mPresentQueue = VK_NULL_HANDLE;

    uint32_t mGraphicsQueueFamilyIndex = 0;
    uint32_t mPresentQueueFamilyIndex = 0;

    std::vector<const char*> getRequiredDeviceExtensions();
};

} // namespace fluidsim

#endif // VULKAN_DEVICE_H
