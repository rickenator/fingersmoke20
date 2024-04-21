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

#define MAX_FRAMES_IN_FLIGHT 2

class VulkanManager {
public:
    VulkanManager(ANativeWindow* window);
    ~VulkanManager();

    int initVulkan();
    void drawFrame();
    void cleanup();

    VkPhysicalDevice pickSuitableDevice(const std::vector<VkPhysicalDevice>& devices,
                                        const std::vector<const char*>& requiredExtensions);
    void createLogicalDevice(const std::vector<const char*>& requiredExtensions);
    bool checkSwapchainSupport(VkPhysicalDevice device);


    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D actualExtent);
    void createSwapChain();
    VkExtent2D getWindowExtent();
    void createGraphicsPipeline();
    void createComputePipeline();
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void createSharedTexture();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void initVulkanFences();
    void initSynchronization();
    void initSemaphores();
    void initImagesInFlight();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void createFramebuffers();


private:
    ANativeWindow* mWindow;
    VkInstance mInstance;
    VkSurfaceKHR mSurface;
    VkDebugUtilsMessengerEXT mDebugMessenger;
    VkPhysicalDevice mPhysicalDevice;
    VkDevice mDevice;
    VkQueue mGraphicsQueue;
    VkQueue mPresentQueue;
    VkSwapchainKHR mSwapChain;
    VkExtent2D mSwapChainExtent;
    std::vector<VkImage> mSwapChainImages;
    std::vector<VkImageView> mSwapChainImageViews;
    VkFormat mSwapChainImageFormat;
    uint32_t mSwapChainImageCount;
    VkRenderPass mRenderPass;
    VkPipeline mGraphicsPipeline;
    VkPipeline mComputePipeline;
    VkImage mTextureImage; // to share between compute and fragment
    std::vector<VkFence> mInFlightFences;
    std::vector<VkFence> mImagesInFlight;
    std::vector<VkSemaphore> mImageAvailableSemaphores;
    std::vector<VkSemaphore> mRenderFinishedSemaphores;
    std::vector<VkCommandBuffer> mCommandBuffers;
    std::vector<VkFramebuffer> mFramebuffers;

 };

#endif //FINGERSMOKE2_0_FS20_H
