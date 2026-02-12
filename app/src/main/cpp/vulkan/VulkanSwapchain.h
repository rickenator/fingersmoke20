#ifndef VULKAN_SWAPCHAIN_H
#define VULKAN_SWAPCHAIN_H

#include <vulkan/vulkan.h>
#include <vector>

namespace fluidsim {

class VulkanSwapchain {
public:
    VulkanSwapchain();
    ~VulkanSwapchain();

    bool createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                         uint32_t width, uint32_t height);
    void destroySwapchain(VkDevice device);

    VkSwapchainKHR getSwapchain() const { return mSwapchain; }
    std::vector<VkImage> getImages() const { return mImages; }
    VkFormat getImageFormat() const { return mImageFormat; }
    uint32_t getImageCount() const { return mImageCount; }

    bool acquireNextImage(VkSemaphore semaphore, uint32_t* imageIndex);
    bool presentFrame(VkQueue queue, VkSemaphore semaphore, uint32_t imageIndex);

private:
    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
    std::vector<VkImage> mImages;
    VkFormat mImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    uint32_t mImageCount = 0;
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
};

} // namespace fluidsim

#endif // VULKAN_SWAPCHAIN_H
