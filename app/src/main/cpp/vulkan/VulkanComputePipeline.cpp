#include "VulkanComputePipeline.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>

namespace fluidsim {

VulkanComputePipeline::VulkanComputePipeline() {
}

VulkanComputePipeline::~VulkanComputePipeline() {
    destroy();
}

bool VulkanComputePipeline::create(VulkanContext* context, const std::string& shaderPath, VkDescriptorSetLayout descriptorSetLayout) {
    mContext = context;

    // Set descriptor set layout
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        mDescriptorSetLayout = descriptorSetLayout;
    } else {
        // Set up descriptor set layout
        if (!setupDescriptorSetLayout()) {
            std::cerr << "Failed to setup descriptor set layout" << std::endl;
            return false;
        }
    }

    // Create shader module
    if (!createShaderModule(shaderPath)) {
        std::cerr << "Failed to create shader module" << std::endl;
        return false;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &mDescriptorSetLayout;

    if (vkCreatePipelineLayout(context->getVulkanCore()->getDevice(), &layoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout" << std::endl;
        return false;
    }

    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = mShaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = mPipelineLayout;

    if (vkCreateComputePipelines(context->getVulkanCore()->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mPipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create compute pipeline" << std::endl;
        return false;
    }

    return true;
}

bool VulkanComputePipeline::setupDescriptorSetLayout() {
    VkDevice device = mContext->getVulkanCore()->getDevice();

    // Create descriptor set layout for storage buffers
    std::array<VkDescriptorSetLayoutBinding, 4> bindings = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[2].pImmutableSamplers = nullptr;

    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[3].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor set layout" << std::endl;
        return false;
    }

    return true;
}

bool VulkanComputePipeline::createDescriptorSet(VkDeviceSize bufferSize) {
    VkDevice device = mContext->getVulkanCore()->getDevice();

    // Set up descriptor set layout
    if (!setupDescriptorSetLayout()) {
        return false;
    }

    // Create descriptor pool
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 4;  // 4 storage buffers + 1 uniform buffer

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor pool" << std::endl;
        return false;
    }

    // Create descriptor set
    VkDescriptorSetLayout descriptorSetLayout = mDescriptorSetLayout;
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &mDescriptorSet) != VK_SUCCESS) {
        std::cerr << "Failed to allocate descriptor set" << std::endl;
        return false;
    }

    // Set up descriptor bindings
    std::array<VkDescriptorBufferInfo, 4> bufferInfos = {};
    // We'll set these up when binding buffers

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.sampler = VK_NULL_HANDLE;

    std::array<VkWriteDescriptorSet, 5> writes = {};

    // Storage buffers (density, velocity X, velocity Y)
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = mDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &bufferInfos[0];

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = mDescriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &bufferInfos[1];

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = mDescriptorSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].pBufferInfo = &bufferInfos[2];

    // Uniform buffer (deltaT, viscosity)
    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = mDescriptorSet;
    writes[3].dstBinding = 3;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[3].pBufferInfo = &bufferInfos[3];

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    return true;
}

void VulkanComputePipeline::setDescriptorSetLayout(VkDescriptorSetLayout descriptorSetLayout) {
    mDescriptorSetLayout = descriptorSetLayout;
}

void VulkanComputePipeline::bindPipeline(VkCommandBuffer commandBuffer) {
    if (mPipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline);
    }
}

void VulkanComputePipeline::dispatch(VkCommandBuffer commandBuffer, uint32_t x, uint32_t y, uint32_t z) {
    vkCmdDispatch(commandBuffer, x, y, z);
}

void VulkanComputePipeline::destroyDescriptorSet() {
    if (mDescriptorSet != VK_NULL_HANDLE && mDescriptorPool != VK_NULL_HANDLE) {
        VkDevice device = mContext->getVulkanCore()->getDevice();
        vkDestroyDescriptorSet(device, mDescriptorSet, nullptr);
        vkDestroyDescriptorPool(device, mDescriptorPool, nullptr);
        mDescriptorSet = VK_NULL_HANDLE;
        mDescriptorPool = VK_NULL_HANDLE;
    }
}

void VulkanComputePipeline::destroy() {
    if (mPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(mContext->getVulkanCore()->getDevice(), mPipeline, nullptr);
        mPipeline = VK_NULL_HANDLE;
    }
    if (mPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(mContext->getVulkanCore()->getDevice(), mPipelineLayout, nullptr);
        mPipelineLayout = VK_NULL_HANDLE;
    }
    destroyShaderModule();
}

bool VulkanComputePipeline::createShaderModule(const std::string& shaderPath) {
    // Load SPIR-V shader file
    std::vector<uint32_t> shaderCode = readShaderFile(shaderPath);

    if (shaderCode.empty()) {
        std::cerr << "Failed to load shader file: " << shaderPath << std::endl;
        return false;
    }

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size() * sizeof(uint32_t);
    createInfo.pCode = shaderCode.data();

    if (vkCreateShaderModule(mContext->getVulkanCore()->getDevice(), &createInfo, nullptr, &mShaderModule) != VK_SUCCESS) {
        std::cerr << "Failed to create shader module" << std::endl;
        return false;
    }

    return true;
}

std::vector<uint32_t> VulkanComputePipeline::readShaderFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // SPIR-V shaders must be aligned to 4 bytes (uint32_t)
    // Read as bytes first, then convert to uint32_t
    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size)) {
        // Ensure size is multiple of 4
        size_t uint32Size = (size + 3) & ~3;
        std::vector<uint32_t> result(uint32Size / 4, 0);
        std::memcpy(result.data(), buffer.data(), size);
        return result;
    }

    return {};
}

void VulkanComputePipeline::destroyShaderModule() {
    if (mShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(mContext->getVulkanCore()->getDevice(), mShaderModule, nullptr);
        mShaderModule = VK_NULL_HANDLE;
    }
}

} // namespace fluidsim
