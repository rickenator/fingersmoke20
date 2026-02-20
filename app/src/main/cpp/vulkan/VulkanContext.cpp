#include "VulkanContext.h"
#include <iostream>

namespace fluidsim {

VulkanContext::VulkanContext() {
}

VulkanContext::~VulkanContext() {
    cleanup();
}

bool VulkanContext::initialize(void* window, int width, int height) {
    mVulkanCore = std::make_unique<VulkanCore>();
    if (!mVulkanCore->initialize((ANativeWindow*)window, width, height)) {
        std::cerr << "Failed to initialize VulkanCore" << std::endl;
        return false;
    }

    mMemoryAllocator = std::make_unique<VulkanMemory>();
    if (!mMemoryAllocator->initialize(mVulkanCore->getDevice(), mVulkanCore->getPhysicalDevice())) {
        std::cerr << "Failed to initialize VulkanMemory" << std::endl;
        return false;
    }

    // Set the graphics queue on the memory allocator for copy operations
    mQueueFamilyIndex = mVulkanCore->getGraphicsQueueFamilyIndex();
    mGraphicsQueue = mVulkanCore->getGraphicsQueue();
    mMemoryAllocator->setQueue(mGraphicsQueue);

    mCommand = std::make_unique<VulkanCommand>();
    if (!mCommand->createCommandPool(mVulkanCore->getDevice(), mVulkanCore->getGraphicsQueueFamilyIndex())) {
        std::cerr << "Failed to create command pool" << std::endl;
        return false;
    }

    return true;
}

void VulkanContext::cleanup() {
    if (mCommand) {
        mCommand->destroyCommandPool(mVulkanCore->getDevice());
        mCommand.reset();
    }
    mMemoryAllocator.reset();
    mVulkanCore.reset();
}

} // namespace fluidsim
