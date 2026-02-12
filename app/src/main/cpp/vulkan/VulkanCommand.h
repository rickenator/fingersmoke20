#ifndef VULKAN_COMMAND_H
#define VULKAN_COMMAND_H

#include <vulkan/vulkan.h>
#include <vector>

namespace fluidsim {

class VulkanCommand {
public:
    VulkanCommand();
    ~VulkanCommand();

    bool createCommandPool(VkDevice device, uint32_t queueFamilyIndex);
    void destroyCommandPool(VkDevice device);

    bool allocateCommandBuffers(VkDevice device, uint32_t count);
    void freeCommandBuffers(VkDevice device);
    void freeCommandBuffer(VkDevice device, VkCommandBuffer buffer);

    bool beginSingleTimeCommands(VkDevice device);
    void endSingleTimeCommands(VkDevice device, VkQueue queue);

    bool submitWork(VkQueue queue, VkCommandBuffer commandBuffer, VkFence fence = VK_NULL_HANDLE);

    VkCommandPool getCommandPool() const { return mCommandPool; }
    const std::vector<VkCommandBuffer>& getCommandBuffers() const { return mCommandBuffers; }
    VkCommandBuffer getPrimaryCommandBuffer() const { return mCommandBuffers.empty() ? VK_NULL_HANDLE : mCommandBuffers[0]; }
    VkQueue getQueue(VkDevice device) const;

private:
    VkCommandPool mCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mCommandBuffers;
    uint32_t mQueueFamilyIndex = 0;
    mutable VkQueue mQueue = VK_NULL_HANDLE;
};

} // namespace fluidsim

#endif // VULKAN_COMMAND_H
