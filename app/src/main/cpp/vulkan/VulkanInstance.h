#ifndef VULKAN_INSTANCE_H
#define VULKAN_INSTANCE_H

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

namespace fluidsim {

class VulkanInstance {
public:
    VulkanInstance();
    ~VulkanInstance();

    bool createInstance();
    void destroyInstance();

    VkInstance getInstance() const { return mInstance; }
    VkPhysicalDevice getPhysicalDevice() const { return mPhysicalDevice; }

    std::vector<const char*> getRequiredInstanceExtensions();
    std::vector<VkPhysicalDevice> enumeratePhysicalDevices();

    bool isDeviceSuitable(VkPhysicalDevice device);
    uint32_t findGraphicsQueueFamily(VkPhysicalDevice device);
    uint32_t findPresentQueueFamily(VkPhysicalDevice device, VkSurfaceKHR surface);

private:
    VkInstance mInstance = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;

    bool validateExtensions();
};

} // namespace fluidsim

#endif // VULKAN_INSTANCE_H
