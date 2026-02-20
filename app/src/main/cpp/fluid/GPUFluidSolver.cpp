#include "GPUFluidSolver.h"
#include "VulkanBuffer.h"
#include "VulkanComputePipeline.h"
#include "VulkanContext.h"
#include <cstring>
#include <stdexcept>
#include <fstream>
#include <android_asset.h>
#include <android/log.h>
#include <random>

#define LOG_TAG "FluidSim"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace fluidsim {

namespace {
struct UniformData {
    float deltaTime;
    float viscosity;
    float resolutionX;
    float resolutionY;
};
} // anonymous namespace

// Helper function to load shader from external file
// Returns nullptr if file cannot be read or has invalid format
static std::vector<char> loadShaderFromFile(AAssetManager* assetManager, const char* path) {
    AAsset* asset = AAssetManager_open(assetManager, path, AASSET_MODE_STREAMING);
    if (!asset) {
        return {};
    }

    off_t assetSize = AAsset_getLength(asset);
    std::vector<char> shaderCode(assetSize);

    int bytes_read = AAsset_read(asset, shaderCode.data(), assetSize);
    AAsset_close(asset);

    if (bytes_read != assetSize) {
        return {};
    }

    return shaderCode;
}

GPUFluidSolver::GPUFluidSolver(int width, int height)
    : mWidth(width), mHeight(height) {
    mDensity = std::make_unique<Grid2D>(width, height);
    mVelocityX = std::make_unique<Grid2D>(width, height);
    mVelocityY = std::make_unique<Grid2D>(width, height);

    mPreviousDensity = std::make_unique<Grid2D>(width, height);
    mPreviousVelocityX = std::make_unique<Grid2D>(width, height);
    mPreviousVelocityY = std::make_unique<Grid2D>(width, height);

    mSolver = std::make_unique<GPULikeFluidSolver>(width, height);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);

    // Initialize with some initial density
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (x > width/4 && x < 3*width/4 && y > height/4 && y < 3*height/4) {
                mDensity->set(x, y, 1.0f);
                mVelocityX->set(x, y, dist(rng));
                mVelocityY->set(x, y, dist(rng));
            }
        }
    }
}

GPUFluidSolver::~GPUFluidSolver() {
    cleanup();
}

void GPUFluidSolver::setContext(VulkanContext* context) {
    mContext = context;
}

void GPUFluidSolver::cleanup() {
    // Cleanup GPU resources
    if (mDensityBuffer) {
        mDensityBuffer->destroy();
        mDensityBuffer.reset();
    }
    if (mVelocityXBuffer) {
        mVelocityXBuffer->destroy();
        mVelocityXBuffer.reset();
    }
    if (mVelocityYBuffer) {
        mVelocityYBuffer->destroy();
        mVelocityYBuffer.reset();
    }
    if (mPreviousDensityBuffer) {
        mPreviousDensityBuffer->destroy();
        mPreviousDensityBuffer.reset();
    }
    if (mPreviousVelocityXBuffer) {
        mPreviousVelocityXBuffer->destroy();
        mPreviousVelocityXBuffer.reset();
    }
    if (mPreviousVelocityYBuffer) {
        mPreviousVelocityYBuffer->destroy();
        mPreviousVelocityYBuffer.reset();
    }
    if (mPressureBuffer) {
        mPressureBuffer->destroy();
        mPressureBuffer.reset();
    }
    if (mForceBuffer) {
        mForceBuffer->destroy();
        mForceBuffer.reset();
    }
    if (mUniformBuffer) {
        mUniformBuffer->destroy();
        mUniformBuffer.reset();
    }

    mDensityBuffer = nullptr;
    mVelocityXBuffer = nullptr;
    mVelocityYBuffer = nullptr;
    mPreviousDensityBuffer = nullptr;
    mPreviousVelocityXBuffer = nullptr;
    mPreviousVelocityYBuffer = nullptr;
    mPressureBuffer = nullptr;
    mForceBuffer = nullptr;
    mUniformBuffer = nullptr;

    if (mDescriptorPool != VK_NULL_HANDLE && mContext) {
        vkDestroyDescriptorPool(mContext->getVulkanCore()->getDevice(), mDescriptorPool, nullptr);
        mDescriptorPool = VK_NULL_HANDLE;
    }
    if (mDescriptorSetLayout != VK_NULL_HANDLE && mContext) {
        vkDestroyDescriptorSetLayout(mContext->getVulkanCore()->getDevice(), mDescriptorSetLayout, nullptr);
        mDescriptorSetLayout = VK_NULL_HANDLE;
    }
    mDensityDescriptorSet = VK_NULL_HANDLE;
    mVelocityXDescriptorSet = VK_NULL_HANDLE;
    mVelocityYDescriptorSet = VK_NULL_HANDLE;

    mContext = nullptr;
}

bool GPUFluidSolver::initialize(VulkanContext* context) {
    setContext(context);

    // Create uniform buffer for simulation parameters
    // We need enough space for deltaT and viscosity
    VkDeviceSize uniformSize = 256;
    mUniformBuffer = std::make_unique<VulkanBuffer>();
    if (!mUniformBuffer->create(context->getVulkanCore()->getDevice(),
                                  context->getVulkanCore()->getPhysicalDevice(),
                                  uniformSize,
                                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        LOGE("Failed to create uniform buffer");
        return false;
    }

    // Create density buffer
    VkDeviceSize densitySize = mWidth * mHeight * sizeof(float);
    mDensityBuffer = std::make_unique<VulkanBuffer>();
    if (!mDensityBuffer->create(context->getVulkanCore()->getDevice(),
                                  context->getVulkanCore()->getPhysicalDevice(),
                                  densitySize,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        LOGE("Failed to create density buffer");
        return false;
    }

    // Create velocity X buffer
    mVelocityXBuffer = std::make_unique<VulkanBuffer>();
    if (!mVelocityXBuffer->create(context->getVulkanCore()->getDevice(),
                                    context->getVulkanCore()->getPhysicalDevice(),
                                    densitySize,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        LOGE("Failed to create velocity X buffer");
        return false;
    }

    // Create velocity Y buffer
    mVelocityYBuffer = std::make_unique<VulkanBuffer>();
    if (!mVelocityYBuffer->create(context->getVulkanCore()->getDevice(),
                                    context->getVulkanCore()->getPhysicalDevice(),
                                    densitySize,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        LOGE("Failed to create velocity Y buffer");
        return false;
    }

    // Create previous density buffer
    mPreviousDensityBuffer = std::make_unique<VulkanBuffer>();
    if (!mPreviousDensityBuffer->create(context->getVulkanCore()->getDevice(),
                                         context->getVulkanCore()->getPhysicalDevice(),
                                         densitySize,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        LOGE("Failed to create previous density buffer");
        return false;
    }

    // Create previous velocity X buffer
    mPreviousVelocityXBuffer = std::make_unique<VulkanBuffer>();
    if (!mPreviousVelocityXBuffer->create(context->getVulkanCore()->getDevice(),
                                            context->getVulkanCore()->getPhysicalDevice(),
                                            densitySize,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        LOGE("Failed to create previous velocity X buffer");
        return false;
    }

    // Create previous velocity Y buffer
    mPreviousVelocityYBuffer = std::make_unique<VulkanBuffer>();
    if (!mPreviousVelocityYBuffer->create(context->getVulkanCore()->getDevice(),
                                            context->getVulkanCore()->getPhysicalDevice(),
                                            densitySize,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        LOGE("Failed to create previous velocity Y buffer");
        return false;
    }

    // Create pressure buffer
    mPressureBuffer = std::make_unique<VulkanBuffer>();
    if (!mPressureBuffer->create(context->getVulkanCore()->getDevice(),
                                  context->getVulkanCore()->getPhysicalDevice(),
                                  densitySize,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        LOGE("Failed to create pressure buffer");
        return false;
    }

    // Create force buffer
    VkDeviceSize forceSize = densitySize;
    mForceBuffer = std::make_unique<VulkanBuffer>();
    if (!mForceBuffer->create(context->getVulkanCore()->getDevice(),
                               context->getVulkanCore()->getPhysicalDevice(),
                               forceSize,
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        LOGE("Failed to create force buffer");
        return false;
    }

    // Initialize buffers with current data
    uploadData();

    // Initialize compute pipelines
    if (!initializePipelines()) {
        LOGE("Failed to initialize compute pipelines");
        return false;
    }

    // Create descriptor sets and bind buffers
    if (!createDescriptorSets()) {
        LOGE("Failed to create descriptor sets");
        return false;
    }

    // Bind buffers to descriptor sets
    bindBuffersToDescriptorSets();

    return true;
}

bool GPUFluidSolver::initializePipelines() {
    if (!mContext) {
        return false;
    }

    // Create shared descriptor set layout for all pipelines
    if (!createDescriptorSetLayout()) {
        return false;
    }

    // Get device and physical device
    VkDevice device = mContext->getVulkanCore()->getDevice();
    VkPhysicalDevice physicalDevice = mContext->getVulkanCore()->getPhysicalDevice();

    // Get shader directory path
    std::string assetDir = "shaders/fluid";

    // Initialize compute pipelines
    mAddForcePipeline = std::make_unique<VulkanComputePipeline>();
    if (!mAddForcePipeline->create(mContext, assetDir + "/fluid_addforce.comp", mDescriptorSetLayout)) {
        LOGE("Failed to create addforce pipeline");
        return false;
    }

    mDiffusePipeline = std::make_unique<VulkanComputePipeline>();
    if (!mDiffusePipeline->create(mContext, assetDir + "/fluid_diffuse.comp", mDescriptorSetLayout)) {
        LOGE("Failed to create diffuse pipeline");
        return false;
    }

    mAdvectPipeline = std::make_unique<VulkanComputePipeline>();
    if (!mAdvectPipeline->create(mContext, assetDir + "/fluid_advect.comp", mDescriptorSetLayout)) {
        LOGE("Failed to create advect pipeline");
        return false;
    }

    mProjectPipeline = std::make_unique<VulkanComputePipeline>();
    if (!mProjectPipeline->create(mContext, assetDir + "/fluid_project.comp", mDescriptorSetLayout)) {
        LOGE("Failed to create project pipeline");
        return false;
    }

    return true;
}

bool GPUFluidSolver::createDescriptorSetLayout() {
    if (!mContext) {
        return false;
    }

    // Create descriptor set layout for compute shaders with 6 bindings
    std::array<VkDescriptorSetLayoutBinding, 6> bindings = {};
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
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[3].pImmutableSamplers = nullptr;

    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[4].pImmutableSamplers = nullptr;

    bindings[5].binding = 5;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[5].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDevice device = mContext->getVulkanCore()->getDevice();
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
        LOGE("Failed to create descriptor set layout");
        return false;
    }

    return true;
}

bool GPUFluidSolver::createDescriptorSets() {
    if (!mContext) {
        return false;
    }

    VkDevice device = mContext->getVulkanCore()->getDevice();
    VkPhysicalDevice physicalDevice = mContext->getVulkanCore()->getPhysicalDevice();

    // Create descriptor pool
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6},  // 6 storage buffers
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}   // 1 uniform buffer
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
        LOGE("Failed to create descriptor pool");
        return false;
    }

    // Create descriptor set (single set for all buffers)
    VkDescriptorSetLayout descriptorSetLayout = mDescriptorSetLayout;
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &mDensityDescriptorSet) != VK_SUCCESS) {
        LOGE("Failed to allocate descriptor set");
        return false;
    }

    mVelocityXDescriptorSet = mDensityDescriptorSet;
    mVelocityYDescriptorSet = mDensityDescriptorSet;

    return true;
}

void GPUFluidSolver::bindBuffersToDescriptorSets() {
    VkDevice device = mContext->getVulkanCore()->getDevice();

    // Create descriptor buffer info for all buffers
    VkDescriptorBufferInfo densityBufferInfo = {};
    densityBufferInfo.buffer = mDensityBuffer->getBuffer();
    densityBufferInfo.offset = 0;
    densityBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo velocityXBufferInfo = {};
    velocityXBufferInfo.buffer = mVelocityXBuffer->getBuffer();
    velocityXBufferInfo.offset = 0;
    velocityXBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo velocityYBufferInfo = {};
    velocityYBufferInfo.buffer = mVelocityYBuffer->getBuffer();
    velocityYBufferInfo.offset = 0;
    velocityYBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo prevDensityBufferInfo = {};
    prevDensityBufferInfo.buffer = mPreviousDensityBuffer->getBuffer();
    prevDensityBufferInfo.offset = 0;
    prevDensityBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo prevVelocityXBufferInfo = {};
    prevVelocityXBufferInfo.buffer = mPreviousVelocityXBuffer->getBuffer();
    prevVelocityXBufferInfo.offset = 0;
    prevVelocityXBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo prevVelocityYBufferInfo = {};
    prevVelocityYBufferInfo.buffer = mPreviousVelocityYBuffer->getBuffer();
    prevVelocityYBufferInfo.offset = 0;
    prevVelocityYBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo uniformBufferInfo = {};
    uniformBufferInfo.buffer = mUniformBuffer->getBuffer();
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = VK_WHOLE_SIZE;

    // Prepare write descriptors
    std::array<VkWriteDescriptorSet, 6> writes = {};

    // Binding 0: Density buffer
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = mDensityDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &densityBufferInfo;

    // Binding 1: Velocity X buffer
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = mDensityDescriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &velocityXBufferInfo;

    // Binding 2: Velocity Y buffer
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = mDensityDescriptorSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].pBufferInfo = &velocityYBufferInfo;

    // Binding 3: Previous density buffer
    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = mDensityDescriptorSet;
    writes[3].dstBinding = 3;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[3].pBufferInfo = &prevDensityBufferInfo;

    // Binding 4: Previous velocity X buffer
    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = mDensityDescriptorSet;
    writes[4].dstBinding = 4;
    writes[4].descriptorCount = 1;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[4].pBufferInfo = &prevVelocityXBufferInfo;

    // Binding 5: Uniform buffer
    writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet = mDensityDescriptorSet;
    writes[5].dstBinding = 5;
    writes[5].descriptorCount = 1;
    writes[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[5].pBufferInfo = &uniformBufferInfo;

    vkUpdateDescriptorSets(device, 6, writes.data(), 0, nullptr);
}

void GPUFluidSolver::bindDescriptorSet(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet) {
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
}

void GPUFluidSolver::backupState() {
    std::memcpy(mPreviousDensity->getData(), mDensity->getData(), mWidth * mHeight * sizeof(float));
    std::memcpy(mPreviousVelocityX->getData(), mVelocityX->getData(), mWidth * mHeight * sizeof(float));
    std::memcpy(mPreviousVelocityY->getData(), mVelocityY->getData(), mWidth * mHeight * sizeof(float));
}

void GPUFluidSolver::uploadData() {
    if (!mContext || !mDensityBuffer || !mVelocityXBuffer || !mVelocityYBuffer) {
        return;
    }

    // Upload density
    mDensityBuffer->copyFrom(mDensity->getData(), mWidth * mHeight * sizeof(float));

    // Upload velocity X
    mVelocityXBuffer->copyFrom(mVelocityX->getData(), mWidth * mHeight * sizeof(float));

    // Upload velocity Y
    mVelocityYBuffer->copyFrom(mVelocityY->getData(), mWidth * mHeight * sizeof(float));

    // Upload previous density
    mPreviousDensityBuffer->copyFrom(mPreviousDensity->getData(), mWidth * mHeight * sizeof(float));

    // Upload previous velocity X
    mPreviousVelocityXBuffer->copyFrom(mPreviousVelocityX->getData(), mWidth * mHeight * sizeof(float));

    // Upload previous velocity Y
    mPreviousVelocityYBuffer->copyFrom(mPreviousVelocityY->getData(), mWidth * mHeight * sizeof(float));

    // Upload force buffer with all zeros initially
    float* forceData = static_cast<float*>(mForceBuffer->map());
    if (forceData) {
        memset(forceData, 0, mWidth * mHeight * sizeof(float));
        mForceBuffer->unmap();
    }

    // Update uniform buffer with simulation parameters
    UniformData uniformData{0.016f, 0.0f, static_cast<float>(mWidth), static_cast<float>(mHeight)};
    mUniformBuffer->copyFrom(&uniformData, sizeof(uniformData));
}

void GPUFluidSolver::downloadData() {
    if (!mContext || !mDensityBuffer || !mVelocityXBuffer || !mVelocityYBuffer) {
        return;
    }

    // Download density
    float* densityData = static_cast<float*>(mDensityBuffer->map());
    if (densityData) {
        memcpy(mDensity->getData(), densityData, mWidth * mHeight * sizeof(float));
        mDensityBuffer->unmap();
    }

    // Download velocity X
    float* velocityXData = static_cast<float*>(mVelocityXBuffer->map());
    if (velocityXData) {
        memcpy(mVelocityX->getData(), velocityXData, mWidth * mHeight * sizeof(float));
        mVelocityXBuffer->unmap();
    }

    // Download velocity Y
    float* velocityYData = static_cast<float*>(mVelocityYBuffer->map());
    if (velocityYData) {
        memcpy(mVelocityY->getData(), velocityYData, mWidth * mHeight * sizeof(float));
        mVelocityYBuffer->unmap();
    }
}

void GPUFluidSolver::step(float dt, float viscosity) {
    if (!mContext) {
        // CPU fallback: sync current state to mSolver, step, then copy results back
        std::memcpy(mSolver->getDensity()->getData(), mDensity->getData(), mWidth * mHeight * sizeof(float));
        std::memcpy(mSolver->getVelocityX()->getData(), mVelocityX->getData(), mWidth * mHeight * sizeof(float));
        std::memcpy(mSolver->getVelocityY()->getData(), mVelocityY->getData(), mWidth * mHeight * sizeof(float));
        mSolver->step(dt, viscosity);
        std::memcpy(mDensity->getData(), mSolver->getDensity()->getData(), mWidth * mHeight * sizeof(float));
        std::memcpy(mVelocityX->getData(), mSolver->getVelocityX()->getData(), mWidth * mHeight * sizeof(float));
        std::memcpy(mVelocityY->getData(), mSolver->getVelocityY()->getData(), mWidth * mHeight * sizeof(float));
        return;
    }

    backupState();
    uploadData();

    // Upload simulation parameters to uniform buffer
    UniformData uniformData{dt, viscosity, static_cast<float>(mWidth), static_cast<float>(mHeight)};
    mUniformBuffer->copyFrom(&uniformData, sizeof(uniformData));

    // Set up command buffer for compute operations
    if (!mContext->getCommandComponent()->beginSingleTimeCommands(mContext->getVulkanCore()->getDevice())) {
        LOGE("Failed to begin single time commands for fluid simulation");
        downloadData();
        return;
    }

    VkDevice device = mContext->getVulkanCore()->getDevice();
    VkQueue queue = mContext->getCommandComponent()->getQueue(device);
    VkCommandBuffer commandBuffer = mContext->getCommandComponent()->getPrimaryCommandBuffer();

    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    // Diffuse step (bind density descriptor set)
    mDiffusePipeline->bindPipeline(commandBuffer);
    bindDescriptorSet(commandBuffer, mDiffusePipeline->getPipelineLayout(), mDensityDescriptorSet);
    mDiffusePipeline->dispatch(commandBuffer, mWidth, mHeight, 1);

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    // Advect step (bind density descriptor set)
    mAdvectPipeline->bindPipeline(commandBuffer);
    bindDescriptorSet(commandBuffer, mAdvectPipeline->getPipelineLayout(), mDensityDescriptorSet);
    mAdvectPipeline->dispatch(commandBuffer, mWidth, mHeight, 1);

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    // Project step (bind velocity descriptor sets)
    mProjectPipeline->bindPipeline(commandBuffer);
    bindDescriptorSet(commandBuffer, mProjectPipeline->getPipelineLayout(), mVelocityXDescriptorSet);
    bindDescriptorSet(commandBuffer, mProjectPipeline->getPipelineLayout(), mVelocityYDescriptorSet);
    mProjectPipeline->dispatch(commandBuffer, mWidth, mHeight, 1);

    // End and submit commands
    mContext->getCommandComponent()->endSingleTimeCommands(device, queue);

    // Synchronize and download results
    downloadData();
}

void GPUFluidSolver::addTouchForce(int x, int y, float radius, float strength) {
    if (!mContext) {
        // Fallback to CPU implementation
        for (int i = x - radius; i <= x + radius; i++) {
            for (int j = y - radius; j <= y + radius; j++) {
                if (i >= 0 && i < mWidth && j >= 0 && j < mHeight) {
                    float dx = (i - x) / radius;
                    float dy = (j - y) / radius;
                    float dist = sqrt(dx * dx + dy * dy);

                    if (dist <= 1.0f) {
                        float falloff = (1.0f - dist) * strength;
                        mVelocityX->set(i, j, mVelocityX->get(i, j) + falloff);
                        mVelocityY->set(i, j, mVelocityY->get(i, j) + falloff);
                    }
                }
            }
        }
        return;
    }

    uploadData();

    // Dispatch compute shader to add force at specified location
    VkDevice device = mContext->getVulkanCore()->getDevice();
    VkQueue queue = mContext->getCommandComponent()->getQueue(device);
    VkCommandBuffer commandBuffer = mContext->getCommandComponent()->getPrimaryCommandBuffer();

    // Set up uniform buffer for force parameters
    struct ForceUniformData {
        float x;
        float y;
        float radius;
        float strength;
    };
    ForceUniformData forceData{static_cast<float>(x), static_cast<float>(y), radius, strength};
    mUniformBuffer->copyFrom(&forceData, sizeof(forceData));

    // Dispatch compute shader for addforce
    if (!mContext->getCommandComponent()->beginSingleTimeCommands(device)) {
        LOGE("Failed to begin single time commands for force application");
        downloadData();
        return;
    }

    // Use addforce pipeline if available, otherwise skip
    if (mAddForcePipeline) {
        mAddForcePipeline->bindPipeline(commandBuffer);
        bindDescriptorSet(commandBuffer, mAddForcePipeline->getPipelineLayout(), mDensityDescriptorSet);
        mAddForcePipeline->dispatch(commandBuffer, mWidth, mHeight, 1);
    } else {
        // If addforce pipeline not created, skip GPU force addition
        // Force will be applied during next step if data is modified on CPU
        LOGE("Warning: addforce pipeline not created, skipping GPU force application");
    }

    mContext->getCommandComponent()->endSingleTimeCommands(device, queue);

    // Download results
    downloadData();
}

void GPUFluidSolver::setDensity(int x, int y, float value) {
    mDensity->set(x, y, value);
}

float GPUFluidSolver::getDensity(int x, int y) const {
    return mDensity->get(x, y);
}

void GPUFluidSolver::setVelocity(int x, int y, float vx, float vy) {
    mVelocityX->set(x, y, vx);
    mVelocityY->set(x, y, vy);
}

void GPUFluidSolver::getVelocity(int x, int y, float& vx, float& vy) const {
    vx = mVelocityX->get(x, y);
    vy = mVelocityY->get(x, y);
}

} // namespace fluidsim