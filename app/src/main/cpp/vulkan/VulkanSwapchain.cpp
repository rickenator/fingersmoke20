#include "VulkanSwapchain.h"
#include <iostream>
#include <vector>

namespace fluidsim {

VulkanSwapchain::VulkanSwapchain() {}

VulkanSwapchain::~VulkanSwapchain() {}

bool VulkanSwapchain::createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                       uint32_t width, uint32_t height) {
    mWidth = width;
    mHeight = height;

    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to query surface capabilities: " << result << std::endl;
        return false;
    }

    // Choose surface format
    std::vector<VkSurfaceFormatKHR> availableFormats;
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    availableFormats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, availableFormats.data());

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(availableFormats);

    // Choose present mode
    std::vector<VkPresentModeKHR> availablePresentModes;
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    availablePresentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, availablePresentModes.data());

    VkPresentModeKHR presentMode = chooseSwapPresentMode(availablePresentModes);

    // Choose extent
    VkExtent2D extent = chooseSwapExtent(capabilities);

    // Determine image count
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // Create swapchainCreateInfo
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Queue family sharing mode
    std::vector<uint32_t> queueFamilyIndices = {0, 0}; // Will be filled by caller
    if (queueFamilyIndices[0] != queueFamilyIndices[1]) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &mSwapchain);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create swapchain: " << result << std::endl;
        return false;
    }

    // Get swapchain images
    vkGetSwapchainImagesKHR(device, mSwapchain, &imageCount, nullptr);
    mImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, mSwapchain, &imageCount, mImages.data());
    mImageCount = imageCount;
    mImageFormat = surfaceFormat.format;

    return true;
}

void VulkanSwapchain::destroySwapchain(VkDevice device) {
    if (mSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }
    mImages.clear();
    mImageCount = 0;
}

bool VulkanSwapchain::acquireNextImage(VkSemaphore semaphore, uint32_t* imageIndex) {
    VkResult result = vkAcquireNextImageKHR(mSwapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, imageIndex);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "Failed to acquire next image: " << result << std::endl;
        return false;
    }
    return true;
}

bool VulkanSwapchain::presentFrame(VkQueue queue, VkSemaphore semaphore, uint32_t imageIndex) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &semaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &mSwapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    VkResult result = vkQueuePresentKHR(queue, &presentInfo);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR) {
        std::cerr << "Failed to present frame: " << result << std::endl;
        return false;
    }
    return true;
}

VkSurfaceFormatKHR VulkanSwapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR VulkanSwapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    // Prefer mailbox for lowest latency
    for (const auto& mode : availablePresentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    // Fallback to fifo which is always available
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != 0xFFFFFFFF && capabilities.currentExtent.height != 0xFFFFFFFF) {
        return capabilities.currentExtent;
    }

    VkExtent2D actualExtent = {mWidth, mHeight};
    actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
    actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
    return actualExtent;
}

} // namespace fluidsim
