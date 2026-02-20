#ifndef VULKAN_COMPUTE_PIPELINE_H
#define VULKAN_COMPUTE_PIPELINE_H

#include "VulkanContext.h"
#include <string>
#include <vector>
#include <cstdint>

namespace fluidsim {

class VulkanComputePipeline {
public:
    VulkanComputePipeline();
    ~VulkanComputePipeline();

    bool create(VulkanContext* context, const std::string& shaderPath, VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE);
    void destroy();

    VkPipeline getPipeline() const { return mPipeline; }
    VkDescriptorSet getDescriptorSet() const { return mDescriptorSet; }
    VkDescriptorPool getDescriptorPool() const { return mDescriptorPool; }
    VkPipelineLayout getPipelineLayout() const { return mPipelineLayout; }

    void setDescriptorSet(VkDescriptorSet descriptorSet) { mDescriptorSet = descriptorSet; }

    void bindPipeline(VkCommandBuffer commandBuffer);
    void dispatch(VkCommandBuffer commandBuffer, uint32_t x, uint32_t y, uint32_t z);

    void setDescriptorSetLayout(VkDescriptorSetLayout descriptorSetLayout);
    VkDescriptorSetLayout getDescriptorSetLayout() const { return mDescriptorSetLayout; }

    // Create descriptor set for compute shader
    bool createDescriptorSet(VkDeviceSize bufferSize);
    void destroyDescriptorSet();

    // Set up descriptor set layout
    bool setupDescriptorSetLayout();

private:
    VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;

private:
    VulkanContext* mContext = nullptr;
    VkPipeline mPipeline = VK_NULL_HANDLE;
    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSet mDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
    VkShaderModule mShaderModule = VK_NULL_HANDLE;

    bool createShaderModule(const std::string& shaderPath);
    void destroyShaderModule();

    std::vector<uint32_t> readShaderFile(const std::string& path);
};

} // namespace fluidsim

#endif // VULKAN_COMPUTE_PIPELINE_H
