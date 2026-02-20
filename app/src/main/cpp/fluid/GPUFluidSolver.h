#ifndef GPU_FLUID_SOLVER_H
#define GPU_FLUID_SOLVER_H

#include "Grid2D.h"
#include "VulkanContext.h"
#include <memory>
#include <vector>

namespace fluidsim {

class GPULikeFluidSolver {
public:
    GPULikeFluidSolver(int width, int height);
    ~GPULikeFluidSolver();

    void backupState();
    void step(float dt, float viscosity);
    void addTouchForce(int x, int y, float radius, float strength);
    void setDensity(int x, int y, float value);
    float getDensity(int x, int y) const;
    void setVelocity(int x, int y, float vx, float vy);
    void getVelocity(int x, int y, float& vx, float& vy) const;

    Grid2D* getDensity() { return mDensity.get(); }
    Grid2D* getVelocityX() { return mVelocityX.get(); }
    Grid2D* getVelocityY() { return mVelocityY.get(); }

private:
    int mWidth, mHeight;
    std::unique_ptr<Grid2D> mDensity, mVelocityX, mVelocityY;
    std::unique_ptr<Grid2D> mPreviousDensity, mPreviousVelocityX, mPreviousVelocityY;

    void diffuse(int b, Grid2D& x, const Grid2D& x0, float dt, float diff);
    void project(Grid2D& velocX, Grid2D& velocY, Grid2D& p, Grid2D& div);
    void advect(int b, Grid2D& d, const Grid2D& d0, const Grid2D& velocX, const Grid2D& velocY, float dt);
    void setBoundary(int b, Grid2D& x);
};

// GPU-based fluid solver using Vulkan compute shaders
class GPUFluidSolver {
public:
    GPUFluidSolver(int width, int height);
    ~GPUFluidSolver();

    bool initialize(VulkanContext* context);
    void setContext(VulkanContext* context);
    void cleanup();

    // Step the simulation with given time step and viscosity
    void step(float dt, float viscosity);

    // Add force at touch position
    void addTouchForce(int x, int y, float radius, float strength);

    // Get density grid for rendering
    Grid2D& getDensity() { return *mDensity; }
    const Grid2D& getDensity() const { return *mDensity; }

    Grid2D& getVelocityX() { return *mVelocityX; }
    const Grid2D& getVelocityX() const { return *mVelocityX; }

    Grid2D& getVelocityY() { return *mVelocityY; }
    const Grid2D& getVelocityY() const { return *mVelocityY; }

    // Individual grid access methods
    void setDensity(int x, int y, float value);
    float getDensity(int x, int y) const;
    void setVelocity(int x, int y, float vx, float vy);
    void getVelocity(int x, int y, float& vx, float& vy) const;

private:
    int mWidth, mHeight;

    // CPU-side grids for rendering (data is updated from GPU)
    std::unique_ptr<Grid2D> mDensity;
    std::unique_ptr<Grid2D> mVelocityX;
    std::unique_ptr<Grid2D> mVelocityY;

    // Previous state for CPU-side data
    std::unique_ptr<Grid2D> mPreviousDensity;
    std::unique_ptr<Grid2D> mPreviousVelocityX;
    std::unique_ptr<Grid2D> mPreviousVelocityY;

    // CPU fallback solver
    std::unique_ptr<GPULikeFluidSolver> mSolver;

    // Back up CPU state before GPU step
    void backupState();

    // Upload CPU data to GPU
    void uploadData();

    // Download GPU data to CPU
    void downloadData();

    // Vulkan context and resources
    VulkanContext* mContext = nullptr;

    // Compute pipelines for fluid simulation
    std::unique_ptr<VulkanComputePipeline> mAddForcePipeline;
    std::unique_ptr<VulkanComputePipeline> mDiffusePipeline;
    std::unique_ptr<VulkanComputePipeline> mAdvectPipeline;
    std::unique_ptr<VulkanComputePipeline> mProjectPipeline;

    // Descriptor sets for compute shaders
    VkDescriptorSet mDensityDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet mVelocityXDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet mVelocityYDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;

    // Shared descriptor set layout for all pipelines
    VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;

    // Bind descriptor set to pipeline
    void bindDescriptorSet(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet);

    // Initialize compute pipelines
    bool initializePipelines();

    // Create descriptor sets and bind buffers
    bool createDescriptorSets();

    // Create shared descriptor set layout for all pipelines
    bool createDescriptorSetLayout();

    // Bind buffers to descriptor sets
    void bindBuffersToDescriptorSets();

    // GPU buffers
    std::unique_ptr<VulkanBuffer> mDensityBuffer;
    std::unique_ptr<VulkanBuffer> mVelocityXBuffer;
    std::unique_ptr<VulkanBuffer> mVelocityYBuffer;
    std::unique_ptr<VulkanBuffer> mPreviousDensityBuffer;
    std::unique_ptr<VulkanBuffer> mPreviousVelocityXBuffer;
    std::unique_ptr<VulkanBuffer> mPreviousVelocityYBuffer;
    std::unique_ptr<VulkanBuffer> mPressureBuffer;
    std::unique_ptr<VulkanBuffer> mForceBuffer;
    std::unique_ptr<VulkanBuffer> mUniformBuffer;
};

} // namespace fluidsim

#endif // GPU_FLUID_SOLVER_H