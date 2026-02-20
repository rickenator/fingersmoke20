#ifndef VULKAN_MEMORY_H
#define VULKAN_MEMORY_H

#include <vulkan/vulkan.h>
#include <memory>

namespace fluidsim {

class VulkanCommand;
class VulkanMemory {
public:
    VulkanMemory();
    ~VulkanMemory();

    bool initialize(VkDevice device, VkPhysicalDevice physicalDevice);
    void cleanup();

    VkDeviceMemory allocateMemory(VkMemoryRequirements requirements, VkMemoryPropertyFlags properties);
    void freeMemory(VkDeviceMemory memory);

    void* mapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size);
    void unmapMemory(VkDeviceMemory memory);

    void setQueue(VkQueue queue) { mQueue = queue; }

    VkQueue getQueue() const { return mQueue; }

    bool copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkCommandPool commandPool);
    bool copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height,
                          VkCommandPool commandPool);

private:
    VkDevice mDevice = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkQueue mQueue = VK_NULL_HANDLE;
    uint32_t mMemoryTypeCount = 0;
    VkPhysicalDeviceMemoryProperties mMemoryProperties{};

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

} // namespace fluidsim

#endif // VULKAN_MEMORY_H
