//
// Created by rick on 4/20/24.
//

#ifndef FINGERSMOKE2_0_FS20_H
#define FINGERSMOKE2_0_FS20_H

#include <jni.h>
#include <android/native_window_jni.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <string>
#include <optional>

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

private:
    ANativeWindow* mWindow;
    VkInstance mInstance;
    VkSurfaceKHR mSurface;
    VkDebugUtilsMessengerEXT mDebugMessenger;
    VkPhysicalDevice mPhysicalDevice;
    VkDevice mDevice;
    VkQueue mGraphicsQueue;
    VkSwapchainKHR mSwapChain;
    VkExtent2D mSwapChainExtent;
    std::vector<VkImage> mSwapChainImages;
    VkFormat mSwapChainImageFormat;
    // Other Vulkan objects like device, surface, swapchain, etc.
};

#endif //FINGERSMOKE2_0_FS20_H
