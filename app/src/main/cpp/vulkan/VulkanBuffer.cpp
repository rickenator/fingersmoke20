#include "VulkanBuffer.h"
#include <cstring>
#include <iostream>

namespace fluidsim {

VulkanBuffer::VulkanBuffer() {
}

VulkanBuffer::~VulkanBuffer() {
    destroy();
}

bool VulkanBuffer::create(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                          VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    mDevice = device;
    mPhysicalDevice = physicalDevice;
    mSize = size;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &mBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to create buffer" << std::endl;
        return false;
    }

    VkMemoryRequirements memRequirements = allocateMemory();

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &mMemory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate buffer memory" << std::endl;
        return false;
    }

    return bindMemory();
}

void VulkanBuffer::destroy() {
    if (mMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mDevice, mMemory, nullptr);
        mMemory = VK_NULL_HANDLE;
    }
    if (mBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(mDevice, mBuffer, nullptr);
        mBuffer = VK_NULL_HANDLE;
    }
    mMapPtr = nullptr;
}

VkMemoryRequirements VulkanBuffer::allocateMemory() {
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(mDevice, mBuffer, &memRequirements);
    return memRequirements;
}

bool VulkanBuffer::bindMemory() {
    if (vkBindBufferMemory(mDevice, mBuffer, mMemory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind buffer memory" << std::endl;
        return false;
    }
    return true;
}

void* VulkanBuffer::map() {
    if (mMapPtr == nullptr) {
        if (vkMapMemory(mDevice, mMemory, 0, mSize, 0, &mMapPtr) != VK_SUCCESS) {
            return nullptr;
        }
    }
    return mMapPtr;
}

void VulkanBuffer::unmap() {
    if (mMapPtr != nullptr) {
        vkUnmapMemory(mDevice, mMemory);
        mMapPtr = nullptr;
    }
}

bool VulkanBuffer::copyFrom(const void* data, VkDeviceSize size) {
    void* mapped = map();
    if (!mapped) return false;

    std::memcpy(mapped, data, size);
    unmap();
    return true;
}

bool VulkanBuffer::copyTo(void* data, VkDeviceSize size) {
    void* mapped = map();
    if (!mapped) return false;

    std::memcpy(data, mapped, size);
    unmap();
    return true;
}

uint32_t VulkanBuffer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

} // namespace fluidsim
