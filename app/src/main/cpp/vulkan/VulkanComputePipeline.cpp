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

bool VulkanComputePipeline::create(VulkanContext* context, const std::string& shaderPath) {
    mContext = context;

    // Create shader module
    if (!createShaderModule(shaderPath)) {
        std::cerr << "Failed to create shader module" << std::endl;
        return false;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 0;  // No descriptor sets for now
    layoutInfo.pSetLayouts = nullptr;

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
