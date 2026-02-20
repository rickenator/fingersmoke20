#include "VulkanDevice.h"
#include <iostream>
#include <vector>

namespace fluidsim {

VulkanDevice::VulkanDevice() {}

VulkanDevice::~VulkanDevice() {
    destroyDevice();
}

bool VulkanDevice::createDevice(VkPhysicalDevice physicalDevice, uint32_t graphicsQueueFamilyIndex, uint32_t presentQueueFamilyIndex) {
    mGraphicsQueueFamilyIndex = graphicsQueueFamilyIndex;
    mPresentQueueFamilyIndex = presentQueueFamilyIndex;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::vector<float> queuePriorities = {1.0f};

    // Graphics queue
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = queuePriorities.data();
    queueCreateInfos.push_back(queueCreateInfo);

    // Present queue (may be same as graphics)
    if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
        VkDeviceQueueCreateInfo presentQueueCreateInfo{};
        presentQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        presentQueueCreateInfo.queueFamilyIndex = presentQueueFamilyIndex;
        presentQueueCreateInfo.queueCount = 1;
        presentQueueCreateInfo.pQueuePriorities = queuePriorities.data();
        queueCreateInfos.push_back(presentQueueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(getRequiredDeviceExtensions().size());
    createInfo.ppEnabledExtensionNames = getRequiredDeviceExtensions().data();

    VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &mDevice);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create logical device: " << result << std::endl;
        return false;
    }

    // Get queues
    vkGetDeviceQueue(mDevice, graphicsQueueFamilyIndex, 0, &mGraphicsQueue);
    if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
        vkGetDeviceQueue(mDevice, presentQueueFamilyIndex, 0, &mPresentQueue);
    } else {
        mPresentQueue = mGraphicsQueue;
    }

    return true;
}

void VulkanDevice::destroyDevice() {
    if (mDevice != VK_NULL_HANDLE) {
        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
        mGraphicsQueue = VK_NULL_HANDLE;
        mPresentQueue = VK_NULL_HANDLE;
    }
}

std::vector<const char*> VulkanDevice::getRequiredDeviceExtensions() {
    return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

} // namespace fluidsim
