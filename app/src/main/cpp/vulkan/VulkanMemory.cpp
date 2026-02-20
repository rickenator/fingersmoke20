#include "VulkanMemory.h"
#include "VulkanCommand.h"
#include <iostream>
#include <stdexcept>

namespace fluidsim {

VulkanMemory::VulkanMemory() {
}

VulkanMemory::~VulkanMemory() {
    cleanup();
}

bool VulkanMemory::initialize(VkDevice device, VkPhysicalDevice physicalDevice) {
    mDevice = device;
    mPhysicalDevice = physicalDevice;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &mMemoryProperties);
    mMemoryTypeCount = mMemoryProperties.memoryTypeCount;
    return true;
}

void VulkanMemory::cleanup() {
}

VkDeviceMemory VulkanMemory::allocateMemory(VkMemoryRequirements requirements, VkMemoryPropertyFlags properties) {
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, properties);

    VkDeviceMemory memory;
    if (vkAllocateMemory(mDevice, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate memory" << std::endl;
        return VK_NULL_HANDLE;
    }
    return memory;
}

void VulkanMemory::freeMemory(VkDeviceMemory memory) {
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(mDevice, memory, nullptr);
    }
}

void* VulkanMemory::mapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size) {
    void* data;
    if (vkMapMemory(mDevice, memory, offset, size, 0, &data) != VK_SUCCESS) {
        return nullptr;
    }
    return data;
}

void VulkanMemory::unmapMemory(VkDeviceMemory memory) {
    vkUnmapMemory(mDevice, memory);
}

uint32_t VulkanMemory::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < mMemoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (mMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

bool VulkanMemory::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkCommandPool commandPool) {
    VulkanCommand command;
    if (!command.createCommandPool(mDevice, 0)) {
        return false;
    }
    if (!command.allocateCommandBuffers(mDevice, 1)) {
        return false;
    }

    VkCommandBuffer commandBuffer = command.getPrimaryCommandBuffer();

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        std::cerr << "Failed to begin command buffer" << std::endl;
        return false;
    }

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;

    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to end command buffer" << std::endl;
        return false;
    }

    // Submit and wait using the queue from the command's device
    VkQueue queue = command.getQueue(mDevice);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        std::cerr << "Failed to submit copy command" << std::endl;
        return false;
    }

    vkQueueWaitIdle(queue);

    // Free the command buffer from the newly created command pool
    vkFreeCommandBuffers(mDevice, command.getCommandPool(), 1, &commandBuffer);
    command.destroyCommandPool(mDevice);

    return true;
}

bool VulkanMemory::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height,
                                     VkCommandPool commandPool) {
    VulkanCommand command;
    if (!command.createCommandPool(mDevice, 0)) {
        return false;
    }
    if (!command.allocateCommandBuffers(mDevice, 1)) {
        return false;
    }

    VkCommandBuffer commandBuffer = command.getPrimaryCommandBuffer();

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        std::cerr << "Failed to begin command buffer" << std::endl;
        return false;
    }

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to end command buffer" << std::endl;
        return false;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Submit and wait using the queue from the command's device
    VkQueue queue = command.getQueue(mDevice);
    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        std::cerr << "Failed to submit copy command" << std::endl;
        return false;
    }

    vkQueueWaitIdle(queue);

    // Free the command buffer from the newly created command pool
    vkFreeCommandBuffers(mDevice, command.getCommandPool(), 1, &commandBuffer);
    command.destroyCommandPool(mDevice);

    return true;
}

} // namespace fluidsim
