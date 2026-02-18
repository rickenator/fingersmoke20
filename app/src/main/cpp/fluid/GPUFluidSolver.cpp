#include "GPUFluidSolver.h"
#include "VulkanBuffer.h"
#include "VulkanComputePipeline.h"
#include "VulkanContext.h"
#include <cstring>
#include <stdexcept>
#include <fstream>
#include <android_asset.h>
#include <iostream>

namespace fluidsim {

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

    // Initialize with some initial density
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (x > width/4 && x < 3*width/4 && y > height/4 && y < 3*height/4) {
                mDensity->set(x, y, 1.0f);
                mVelocityX->set(x, y, (rand() / float(RAND_MAX)) - 0.5f);
                mVelocityY->set(x, y, (rand() / float(RAND_MAX)) - 0.5f);
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
        std::cerr << "Failed to create uniform buffer" << std::endl;
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
        std::cerr << "Failed to create density buffer" << std::endl;
        return false;
    }

    // Create velocity X buffer
    mVelocityXBuffer = std::make_unique<VulkanBuffer>();
    if (!mVelocityXBuffer->create(context->getVulkanCore()->getDevice(),
                                    context->getVulkanCore()->getPhysicalDevice(),
                                    densitySize,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        std::cerr << "Failed to create velocity X buffer" << std::endl;
        return false;
    }

    // Create velocity Y buffer
    mVelocityYBuffer = std::make_unique<VulkanBuffer>();
    if (!mVelocityYBuffer->create(context->getVulkanCore()->getDevice(),
                                    context->getVulkanCore()->getPhysicalDevice(),
                                    densitySize,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        std::cerr << "Failed to create velocity Y buffer" << std::endl;
        return false;
    }

    // Create previous density buffer
    mPreviousDensityBuffer = std::make_unique<VulkanBuffer>();
    if (!mPreviousDensityBuffer->create(context->getVulkanCore()->getDevice(),
                                         context->getVulkanCore()->getPhysicalDevice(),
                                         densitySize,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        std::cerr << "Failed to create previous density buffer" << std::endl;
        return false;
    }

    // Create previous velocity X buffer
    mPreviousVelocityXBuffer = std::make_unique<VulkanBuffer>();
    if (!mPreviousVelocityXBuffer->create(context->getVulkanCore()->getDevice(),
                                            context->getVulkanCore()->getPhysicalDevice(),
                                            densitySize,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        std::cerr << "Failed to create previous velocity X buffer" << std::endl;
        return false;
    }

    // Create previous velocity Y buffer
    mPreviousVelocityYBuffer = std::make_unique<VulkanBuffer>();
    if (!mPreviousVelocityYBuffer->create(context->getVulkanCore()->getDevice(),
                                            context->getVulkanCore()->getPhysicalDevice(),
                                            densitySize,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        std::cerr << "Failed to create previous velocity Y buffer" << std::endl;
        return false;
    }

    // Create pressure buffer
    mPressureBuffer = std::make_unique<VulkanBuffer>();
    if (!mPressureBuffer->create(context->getVulkanCore()->getDevice(),
                                  context->getVulkanCore()->getPhysicalDevice(),
                                  densitySize,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        std::cerr << "Failed to create pressure buffer" << std::endl;
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
        std::cerr << "Failed to create force buffer" << std::endl;
        return false;
    }

    // Initialize buffers with current data
    uploadData();

    return true;
}

GPUFluidSolver::~GPUFluidSolver() {
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
    struct UniformData {
        float deltaTime;
        float viscosity;
        float resolutionX;
        float resolutionY;
    };
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
        // Fallback to CPU if no Vulkan context available
        Solver solver(mWidth, mHeight);
        solver.diffuse(0, *mDensity, *mPreviousDensity, viscosity, dt);
        solver.diffuse(1, *mVelocityX, *mPreviousVelocityX, viscosity, dt);
        solver.diffuse(2, *mVelocityY, *mPreviousVelocityY, viscosity, dt);

        solver.project(*mVelocityX, *mVelocityY, *mPreviousVelocityX, *mPreviousVelocityY);

        solver.advect(0, *mDensity, *mPreviousDensity, *mPreviousDensity, *mVelocityX, *mVelocityY, dt);
        solver.advect(1, *mVelocityX, *mPreviousVelocityX, *mDensity, *mVelocityX, *mVelocityY, dt);
        solver.advect(2, *mVelocityY, *mPreviousVelocityY, *mDensity, *mVelocityX, *mVelocityY, dt);

        solver.project(*mVelocityX, *mVelocityY, *mPreviousVelocityX, *mPreviousVelocityY);
        return;
    }

    backupState();
    uploadData();

    // In a full implementation, this would:
    // 1. Create and compile Vulkan compute shaders
    // 2. Set up uniform buffers with simulation parameters
    // 3. Dispatch compute workgroups for each step:
    //    - Diffuse: solve diffusion equation
    //    - Advect: backtrace and interpolate
    //    - Project: solve Poisson equation for pressure
    // 4. Apply boundary conditions
    // 5. Synchronize and download results

    // For now, download and return
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

    // In full implementation, this would:
    // 1. Dispatch compute shader to add force at specified location
    // 2. Use radial falloff for smooth force distribution
    // 3. Download results

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