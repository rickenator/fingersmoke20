#ifndef VULKAN_BUFFER_H
#define VULKAN_BUFFER_H

#include <vulkan/vulkan.h>
#include <memory>

namespace fluidsim {

class VulkanBuffer {
public:
    VulkanBuffer();
    ~VulkanBuffer();

    bool create(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void destroy();

    VkBuffer getBuffer() const { return mBuffer; }
    VkDeviceMemory getMemory() const { return mMemory; }
    VkDeviceSize getSize() const { return mSize; }

    void* map();
    void unmap();

    bool copyFrom(const void* data, VkDeviceSize size);
    bool copyTo(void* data, VkDeviceSize size);

private:
    VkDevice mDevice = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkBuffer mBuffer = VK_NULL_HANDLE;
    VkDeviceMemory mMemory = VK_NULL_HANDLE;
    VkDeviceSize mSize = 0;
    void* mMapPtr = nullptr;

    VkMemoryRequirements allocateMemory();
    bool bindMemory();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

} // namespace fluidsim

#endif // VULKAN_BUFFER_H
