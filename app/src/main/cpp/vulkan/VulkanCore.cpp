#include "VulkanCore.h"
#include <iostream>
#include <vector>

namespace fluidsim {

VulkanCore::VulkanCore() {}

VulkanCore::~VulkanCore() {
    cleanup();
}

VkInstance VulkanCore::getInstance() const {
    return mInstance ? mInstance->getInstance() : VK_NULL_HANDLE;
}

VkDevice VulkanCore::getDevice() const {
    return mDevice ? mDevice->getDevice() : VK_NULL_HANDLE;
}

VkSurfaceKHR VulkanCore::getSurface() const {
    return mSurface ? mSurface->getSurface() : VK_NULL_HANDLE;
}

VkQueue VulkanCore::getGraphicsQueue() const {
    return mDevice ? mDevice->getGraphicsQueue() : VK_NULL_HANDLE;
}

VkQueue VulkanCore::getPresentQueue() const {
    return mDevice ? mDevice->getPresentQueue() : VK_NULL_HANDLE;
}

uint32_t VulkanCore::getGraphicsQueueFamilyIndex() const {
    return mDevice ? mDevice->getGraphicsQueueFamilyIndex() : 0;
}

uint32_t VulkanCore::getPresentQueueFamilyIndex() const {
    return mDevice ? mDevice->getPresentQueueFamilyIndex() : 0;
}

bool VulkanCore::initialize(ANativeWindow* window, int width, int height) {
    mWidth = width;
    mHeight = height;

    // Create instance
    mInstance = std::make_unique<VulkanInstance>();
    if (!mInstance->createInstance()) {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return false;
    }
    mPhysicalDevice = mInstance->getPhysicalDevice();

    // Create surface
    mSurface = std::make_unique<VulkanSurface>();
    if (!mSurface->createSurface(mInstance->getInstance(), window)) {
        std::cerr << "Failed to create surface" << std::endl;
        return false;
    }

    // Find queue family indices
    uint32_t graphicsQueueFamilyIndex = mInstance->findGraphicsQueueFamily(mPhysicalDevice);
    uint32_t presentQueueFamilyIndex = mInstance->findPresentQueueFamily(mPhysicalDevice, mSurface->getSurface());

    if (graphicsQueueFamilyIndex == UINT32_MAX || presentQueueFamilyIndex == UINT32_MAX) {
        std::cerr << "Failed to find suitable queue families" << std::endl;
        return false;
    }

    // Create device
    mDevice = std::make_unique<VulkanDevice>();
    if (!mDevice->createDevice(mPhysicalDevice, graphicsQueueFamilyIndex, presentQueueFamilyIndex)) {
        std::cerr << "Failed to create logical device" << std::endl;
        return false;
    }

    // Create swapchain
    mSwapchain = std::make_unique<VulkanSwapchain>();
    if (!mSwapchain->createSwapchain(mDevice->getDevice(), mPhysicalDevice, mSurface->getSurface(), width, height)) {
        std::cerr << "Failed to create swapchain" << std::endl;
        return false;
    }

    // Create command pool
    mCommand = std::make_unique<VulkanCommand>();
    if (!mCommand->createCommandPool(mDevice->getDevice(), graphicsQueueFamilyIndex)) {
        std::cerr << "Failed to create command pool" << std::endl;
        return false;
    }

    return true;
}

void VulkanCore::cleanup() {
    if (mCommand) {
        mCommand->destroyCommandPool(mDevice->getDevice());
        mCommand.reset();
    }

    if (mSwapchain) {
        mSwapchain->destroySwapchain(mDevice->getDevice());
        mSwapchain.reset();
    }

    if (mDevice) {
        mDevice->destroyDevice();
        mDevice.reset();
    }

    if (mSurface) {
        mSurface->destroySurface(mInstance->getInstance());
        mSurface.reset();
    }

    if (mInstance) {
        mInstance->destroyInstance();
        mInstance.reset();
    }
}

bool VulkanCore::createCommandPool(uint32_t queueFamilyIndex) {
    if (!mCommand) {
        mCommand = std::make_unique<VulkanCommand>();
    }
    return mCommand->createCommandPool(mDevice->getDevice(), queueFamilyIndex);
}

void VulkanCore::destroyCommandPool() {
    if (mCommand) {
        mCommand->destroyCommandPool(mDevice->getDevice());
    }
}

bool VulkanCore::allocateCommandBuffers(uint32_t count) {
    if (!mCommand) {
        mCommand = std::make_unique<VulkanCommand>();
    }
    return mCommand->allocateCommandBuffers(mDevice->getDevice(), count);
}

void VulkanCore::freeCommandBuffers() {
    if (mCommand) {
        mCommand->freeCommandBuffers(mDevice->getDevice());
    }
}

bool VulkanCore::beginSingleTimeCommands() {
    if (!mCommand) {
        mCommand = std::make_unique<VulkanCommand>();
    }
    return mCommand->beginSingleTimeCommands(mDevice->getDevice());
}

void VulkanCore::endSingleTimeCommands() {
    if (mCommand) {
        mCommand->endSingleTimeCommands(mDevice->getDevice(), mDevice->getGraphicsQueue());
    }
}

uint32_t VulkanCore::acquireNextImage(VkSemaphore semaphore) {
    uint32_t imageIndex = 0;
    if (mSwapchain && mSwapchain->acquireNextImage(semaphore, &imageIndex)) {
        return imageIndex;
    }
    return UINT32_MAX;
}

void VulkanCore::presentFrame(VkSemaphore semaphore, uint32_t imageIndex) {
    if (mSwapchain) {
        mSwapchain->presentFrame(mDevice->getPresentQueue(), semaphore, imageIndex);
    }
}

bool VulkanCore::isDeviceSuitable(VkPhysicalDevice device) {
    if (mInstance) {
        return mInstance->isDeviceSuitable(device);
    }
    return false;
}

bool VulkanCore::findQueueFamilies(VkPhysicalDevice device) {
    if (mInstance) {
        uint32_t graphicsFamily = mInstance->findGraphicsQueueFamily(device);
        uint32_t presentFamily = mInstance->findPresentQueueFamily(device, mSurface ? mSurface->getSurface() : VK_NULL_HANDLE);
        return graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX;
    }
    return false;
}

std::vector<const char*> VulkanCore::getRequiredInstanceExtensions() {
    if (mInstance) {
        return mInstance->getRequiredInstanceExtensions();
    }
    return {};
}

std::vector<const char*> VulkanCore::getRequiredDeviceExtensions() {
    if (mDevice) {
        return mDevice->getRequiredDeviceExtensions();
    }
    return {};
}

VkSurfaceFormatKHR VulkanCore::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR VulkanCore::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& mode : availablePresentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanCore::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != 0xFFFFFFFF && capabilities.currentExtent.height != 0xFFFFFFFF) {
        return capabilities.currentExtent;
    }

    VkExtent2D actualExtent = {static_cast<uint32_t>(mWidth), static_cast<uint32_t>(mHeight)};
    actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
    actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
    return actualExtent;
}

} // namespace fluidsim
