//
// Created by rick on 4/20/24.
//

#ifndef FINGERSMOKE2_0_FS20_H
#define FINGERSMOKE2_0_FS20_H

#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <Vertex.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <string>
#include <optional>
#include <fstream>
#include <stdexcept>
#include <array>

#define MAX_FRAMES_IN_FLIGHT 2

class VulkanManager {
public:
    VulkanManager(ANativeWindow* window);
    ~VulkanManager();

    int initVulkan();
    void cleanup();

    VkPhysicalDevice pickSuitableDevice(const std::vector<VkPhysicalDevice>& devices,
                                        const std::vector<const char*>& requiredExtensions);
    void createLogicalDevice(const std::vector<const char*>& requiredExtensions);
    bool checkSwapchainSupport(VkPhysicalDevice device);


    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> computeFamily;  // Dedicated compute family

        bool isComplete() const {
            // For basic functionality, ensure graphics and present families are available.
            // Compute family is optional unless specifically required for separate compute operations.
            return graphicsFamily.has_value() && presentFamily.has_value() && (computeFamily.has_value() || computeFamily == graphicsFamily);
        }
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct PushConstantData {
        float deltaTime;
        float visc;
        int width;
        int height;
        glm::vec2 touchPos;
        bool isTouching;
    };


    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D actualExtent);
    void createSwapChain();
    void cleanupSwapChain();
    void recreateSwapChain();
    VkExtent2D getWindowExtent();
    void createGraphicsPipeline();
    void createComputePipeline();
    void setupComputeDescriptorSet();
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void createSharedTexture();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void initVulkanFences();
    void initSynchronization();
    void initSemaphores();
    void initImagesInFlight();
    void recordComputeOperations(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void createCommandBufferForCompute();
    void createFramebuffers();
    void updateTouch(float x, float y, bool isTouching);
    void createPipelineLayout();
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                     VkMemoryPropertyFlags properties,
                                     VkBuffer& buffer,
                                     VkDeviceMemory& bufferMemory);
    void createShaderBuffers();
    void drawFrame(float delta, float x, float y, bool isTouching);


private:
    ANativeWindow* mWindow;
    VkInstance mInstance;
    VkSurfaceKHR mSurface;
    VkDebugUtilsMessengerEXT mDebugMessenger;
    VkPhysicalDevice mPhysicalDevice;
    VkDevice mDevice;

    VkQueue mGraphicsQueue;
    VkQueue mPresentQueue;
    VkQueue mComputeQueue;

    VkSwapchainKHR mSwapChain;
    VkExtent2D mSwapChainExtent;
    std::vector<VkImage> mSwapChainImages;
    std::vector<VkImageView> mSwapChainImageViews;
    VkFormat mSwapChainImageFormat;
    uint32_t mSwapChainImageCount;

    VkRenderPass mRenderPass;
    VkPipeline mGraphicsPipeline;

    VkPipeline mComputePipeline;
    VkPipelineLayout mComputePipelineLayout;

    VkImage mTextureImage; // to share between compute and fragment

    std::vector<VkFence> mInFlightFences;
    std::vector<VkFence> mImagesInFlight;
    std::vector<VkSemaphore> mImageAvailableSemaphores;
    std::vector<VkSemaphore> mRenderFinishedSemaphores;

    std::vector<VkCommandBuffer> mCommandBuffers;
    VkCommandBuffer mComputeCommandBuffer;
    VkCommandPool mComputeCommandPool;

    VkDescriptorSetLayout mDescriptorSetLayout;
    VkDescriptorPool mDescriptorPool;
    VkDescriptorSet mDescriptorSet;
    std::vector<VkFramebuffer> mFramebuffers;

    VkBuffer mVelocityBuffer;
    VkDeviceMemory mVelocityBufferMemory;

    VkBuffer mPressureBuffer;
    VkDeviceMemory mPressureBufferMemory;

    VkBuffer mVelocityOutputBuffer;
    VkDeviceMemory mVelocityOutputBufferMemory;

    VkBuffer mPressureOutputBuffer;
    VkDeviceMemory mPressureOutputBufferMemory;

 };

#endif //FINGERSMOKE2_0_FS20_H
