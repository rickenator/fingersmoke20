#include "VulkanCommand.h"
#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>

namespace fluidsim {

VulkanCommand::VulkanCommand() {}

VulkanCommand::~VulkanCommand() {}

bool VulkanCommand::createCommandPool(VkDevice device, uint32_t queueFamilyIndex) {
    mQueueFamilyIndex = queueFamilyIndex;

    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.queueFamilyIndex = queueFamilyIndex;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult result = vkCreateCommandPool(device, &createInfo, nullptr, &mCommandPool);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create command pool: " << result << std::endl;
        return false;
    }

    return true;
}

void VulkanCommand::destroyCommandPool(VkDevice device) {
    if (mCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, mCommandPool, nullptr);
        mCommandPool = VK_NULL_HANDLE;
    }
    mCommandBuffers.clear();
}

bool VulkanCommand::allocateCommandBuffers(VkDevice device, uint32_t count) {
    if (mCommandPool == VK_NULL_HANDLE) {
        std::cerr << "Command pool not created" << std::endl;
        return false;
    }

    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = mCommandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = count;

    mCommandBuffers.resize(count);
    VkResult result = vkAllocateCommandBuffers(device, &allocateInfo, mCommandBuffers.data());
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers: " << result << std::endl;
        mCommandBuffers.clear();
        return false;
    }

    return true;
}

void VulkanCommand::freeCommandBuffers(VkDevice device) {
    if (!mCommandBuffers.empty() && mCommandPool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device, mCommandPool, static_cast<uint32_t>(mCommandBuffers.size()), mCommandBuffers.data());
        mCommandBuffers.clear();
    }
}

void VulkanCommand::freeCommandBuffer(VkDevice device, VkCommandBuffer buffer) {
    if (buffer != VK_NULL_HANDLE && mCommandPool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device, mCommandPool, 1, &buffer);
    }
}

bool VulkanCommand::beginSingleTimeCommands(VkDevice device) {
    if (mCommandBuffers.empty()) {
        if (!allocateCommandBuffers(device, 1)) {
            return false;
        }
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult result = vkBeginCommandBuffer(mCommandBuffers[0], &beginInfo);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to begin command buffer: " << result << std::endl;
        return false;
    }

    return true;
}

void VulkanCommand::endSingleTimeCommands(VkDevice device, VkQueue queue) {
    if (mCommandBuffers.empty() || mCommandPool == VK_NULL_HANDLE) {
        return;
    }

    VkCommandBuffer commandBuffer = mCommandBuffers[0];

    VkResult result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to end command buffer: " << result << std::endl;
        return;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to submit command buffer: " << result << std::endl;
        return;
    }

    vkQueueWaitIdle(queue);

    vkResetCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES);
}

bool VulkanCommand::submitWork(VkQueue queue, VkCommandBuffer commandBuffer, VkFence fence) {
    if (commandBuffer == VK_NULL_HANDLE) {
        return false;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkResult result = vkQueueSubmit(queue, 1, &submitInfo, fence);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to submit work: " << result << std::endl;
        return false;
    }

    return true;
}

VkQueue VulkanCommand::getQueue(VkDevice device) const {
    if (mQueue == VK_NULL_HANDLE) {
        vkGetDeviceQueue(device, mQueueFamilyIndex, 0, &mQueue);
    }
    return mQueue;
}

void VulkanCommand::dispatch(VkPipelineLayout pipelineLayout, VkPipeline pipeline, uint32_t x, uint32_t y, uint32_t z) {
    if (mCommandBuffers.empty() || mCommandBuffers[0] == VK_NULL_HANDLE) {
        std::cerr << "No command buffer available for dispatch" << std::endl;
        return;
    }

    VkCommandBuffer commandBuffer = mCommandBuffers[0];

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &mDescriptorSet, 0, nullptr);
    vkCmdDispatch(commandBuffer, x, y, z);
}

} // namespace fluidsim
