#ifndef VULKAN_CONTEXT_H
#define VULKAN_CONTEXT_H

#include "VulkanCore.h"
#include "VulkanBuffer.h"
#include "VulkanMemory.h"
#include "VulkanComputePipeline.h"
#include "VulkanCommand.h"
#include <memory>

namespace fluidsim {

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    bool initialize(void* window, int width, int height);
    void cleanup();

    VulkanCore* getVulkanCore() { return mVulkanCore.get(); }
    VulkanMemory* getMemoryAllocator() { return mMemoryAllocator.get(); }
    VulkanCommand* getCommandComponent() { return mCommand.get(); }

    bool createCommandPool();
    void destroyCommandPool();
    VkCommandPool getCommandPool() const { return mCommandPool; }

private:
    std::unique_ptr<VulkanCore> mVulkanCore;
    std::unique_ptr<VulkanMemory> mMemoryAllocator;
    std::unique_ptr<VulkanCommand> mCommand;
    VkCommandPool mCommandPool = VK_NULL_HANDLE;
    VkQueue mGraphicsQueue = VK_NULL_HANDLE;
    uint32_t mQueueFamilyIndex = 0;
};

} // namespace fluidsim

#endif // VULKAN_CONTEXT_H
