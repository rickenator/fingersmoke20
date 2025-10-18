//
// Created by rick on 4/20/24.
//

#include "fs20.h"

#include <algorithm>
#include <cstring>


namespace {
struct SimpleVertex {
    glm::vec2 pos;
    glm::vec2 uv;
};

VkVertexInputBindingDescription getSimpleBindingDescription() {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(SimpleVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

std::array<VkVertexInputAttributeDescription, 2> getSimpleAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 2> attributes{};
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[0].offset = offsetof(SimpleVertex, pos);

    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[1].offset = offsetof(SimpleVertex, uv);
    return attributes;
}
}

VulkanManager::VulkanManager(JavaVM* jvm, jobject globalActivityRef, ANativeWindow *window, AAssetManager* assetManager)
        : mJvm(jvm), mActivity(globalActivityRef), mWindow(window), mAssetManager(assetManager), mInstance(VK_NULL_HANDLE),
          mSurface(VK_NULL_HANDLE), mDebugMessenger(VK_NULL_HANDLE), mPhysicalDevice(VK_NULL_HANDLE), mDevice(VK_NULL_HANDLE),
          mGraphicsQueue(VK_NULL_HANDLE), mPresentQueue(VK_NULL_HANDLE), mComputeQueue(VK_NULL_HANDLE),
          mSwapChain(VK_NULL_HANDLE), mSwapChainImageFormat(VK_FORMAT_UNDEFINED), mSwapChainImageCount(0),
          mRenderPass(VK_NULL_HANDLE), mGraphicsPipeline(VK_NULL_HANDLE), mGraphicsPipelineLayout(VK_NULL_HANDLE),
          mComputePipeline(VK_NULL_HANDLE), mComputePipelineLayout(VK_NULL_HANDLE), mTextureImage(VK_NULL_HANDLE),
          mTextureImageMemory(VK_NULL_HANDLE), mCommandPool(VK_NULL_HANDLE), mComputeCommandPool(VK_NULL_HANDLE),
          mDescriptorSetLayout(VK_NULL_HANDLE), mDescriptorPool(VK_NULL_HANDLE), mDescriptorSet(VK_NULL_HANDLE),
          mVertexBuffer(VK_NULL_HANDLE), mVertexBufferMemory(VK_NULL_HANDLE), mVertexCount(0) {}

VulkanManager::~VulkanManager() {
    cleanup();
}

static VulkanManager *vkManager = nullptr;  // Global pointer to manage Vulkan lifecycle

// Callback function for Debug Messenger
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}


int VulkanManager::initVulkan() {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanManager";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;  // Or whatever version you're targeting

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    const std::vector<const char *> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char *> extensions = {
#ifdef VKUTILDEBUG
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };


    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (checkLayerSupport() == VK_SUCCESS) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }
    VkResult result = vkCreateInstance(&createInfo, nullptr, &mInstance);
    if (result != VK_SUCCESS) {
        LOGE("Failed to create Vulkan instance! %d",result);
        return -1;
    }

    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.window = mWindow;
    if (vkCreateAndroidSurfaceKHR(mInstance, &surfaceCreateInfo, nullptr, &mSurface) != VK_SUCCESS) {
        std::cerr << "Failed to create Android surface!" << std::endl;
        return -1;
    }

#ifdef VKUTILDEBUG
    // Setup Debug Messenger
    // load the EXT
    auto CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(mInstance, "vkCreateDebugUtilsMessengerEXT");
    if (!CreateDebugUtilsMessengerEXT) {
        throw std::runtime_error("Could not load the vkCreateDebugUtilsMessengerEXT function.");
    }

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debugCreateInfo.pfnUserCallback = debugCallback;

    //if (CreateDebugUtilsMessengerEXT(mInstance, &debugCreateInfo, nullptr, &mDebugMessenger) != VK_SUCCESS) {
     //   throw std::runtime_error("failed to set up debug messenger!");
    //}
#endif
    // Select Physical Device

    // Find all GPU with Vulkan support
    uint32_t deviceCount = 0;

    vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);

    // now fill devices
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());
    LOGI("There are %d devices",deviceCount);

    std::vector<const char*> requiredExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
            VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME
    };

    VkPhysicalDevice selectedDevice = pickSuitableDevice(devices, requiredExtensions);
    if (selectedDevice == VK_NULL_HANDLE) {
        // take the first one
        if (deviceCount > 0) {
            selectedDevice = devices[0];
        } else {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    mPhysicalDevice = selectedDevice;
    //checkDeviceExtensionSupport(mPhysicalDevice, requiredExtensions);

    QueueFamilyIndices surfaceIndices = findQueueFamilies(mPhysicalDevice, mSurface);
    if (!surfaceIndices.presentFamily.has_value()) {
        LOGE("Failed to locate a present queue for the selected device");
        return -1;
    }

    // Check if the surface is supported by the physical device
    VkBool32 surfaceSupported = VK_FALSE;
    result = vkGetPhysicalDeviceSurfaceSupportKHR(
            mPhysicalDevice,
            surfaceIndices.presentFamily.value(),
            mSurface,
            &surfaceSupported);
    if (result != VK_SUCCESS || !surfaceSupported) {
        LOGE("Surface is not supported by the physical device: %d", result);
        return -1;
    }

    // create mDevice
    createLogicalDevice(requiredExtensions);

    // Query the surface capabilities
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &surfaceCapabilities);
    if (result != VK_SUCCESS) {
        LOGE("Failed to get surface capabilities: %d", result);
        return -1;
    }

    LOGI("Surface capabilities retrieved successfully. Min image count: %u, Max image count: %u",
         surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);


    if (checkSwapchainSupport(mPhysicalDevice)) {
        createSwapChain();
    } else {
        throw std::runtime_error("failed to create Swap Chain!");
    }

    createRenderPass();
    createPipelineLayout();
    createGraphicsPipeline();
    createComputePipeline();
    createDescriptorPool();
    createSharedTexture();
    initVulkanFences();
    initSynchronization();
    initSemaphores();
    initImagesInFlight();
    createFramebuffers();
    createShaderBuffers();
    setupComputeDescriptorSet();
    createCommandPool();
    createVertexBuffer();
    createCommandBuffers();
    createCommandBufferForCompute();

    // Notify client that Vulkan is initialized
    notifyClient();

    return 0;
}

bool VulkanManager::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    //std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        LOGI( "Available extension: %s", extension.extensionName);
        //requiredExtensions.erase(extension.extensionName);
    }

    //if (!requiredExtensions.empty()) {
        //for (const auto& missing : requiredExtensions) {
            //std::cerr << "Missing extension: " << missing << "\n";
        //}
        //return false;
    //}
    return true;
}

std::vector<VkLayerProperties> VulkanManager::getAvailableLayers() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    return availableLayers;
}

bool VulkanManager::isLayerAvailable(const char* layerName, const std::vector<VkLayerProperties>& availableLayers) {
    for (const auto& layerProperties : availableLayers) {
        if (strcmp(layerName, layerProperties.layerName) == 0) {
            return true;
        }
    }
    return false;
}

VkResult VulkanManager::checkLayerSupport() {
    const std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
    };

    auto availableLayers = getAvailableLayers();
    for (const auto& layerName : validationLayers) {
        if (!isLayerAvailable(layerName, availableLayers)) {
            LOGE( "Layer not available: %s", layerName);
            return VK_ERROR_LAYER_NOT_PRESENT;
        } else {
            LOGI( "Layer available: %s", layerName);
        }
    }
    return VK_SUCCESS;
}

std::vector<VkExtensionProperties> VulkanManager::getAvailableExtensions(VkPhysicalDevice device) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr,
                                         &extensionCount, nullptr);  // Get the count

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());  // Fetch the extensions
    return extensions;
}

std::vector<const char*> VulkanManager::getValidationLayers() {
    const std::vector<const char*> desiredLayers = {
            "VK_LAYER_KHRONOS_validation"
    };

    auto availableLayers = getAvailableLayers();
    std::vector<const char*> enabledLayers;

    for (const auto& layer : desiredLayers) {
        if (isLayerAvailable(layer, availableLayers)) {
            enabledLayers.push_back(layer);
        } else {
            __android_log_print(ANDROID_LOG_WARN, "VulkanSetup", "Validation layer unavailable: %s", layer);
        }
    }
    return enabledLayers;
}

bool VulkanManager::checkDeviceExtensionSupport(VkPhysicalDevice device,
                                 const std::vector<const char*>& requiredExtensions) {
    std::vector<VkExtensionProperties> availableExtensions = getAvailableExtensions(device);

    std::set<std::string> requiredExtensionsSet(requiredExtensions.begin(), requiredExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensionsSet.erase(extension.extensionName);
    }

    return requiredExtensionsSet.empty();  // Returns true if all required extensions are supported by the device
}

VkPhysicalDevice VulkanManager::pickSuitableDevice(const std::vector<VkPhysicalDevice>& devices,
                                                   const std::vector<const char*>& requiredExtensions) {
    for (const auto& device : devices) {
        LOGI("Found a device");
        if (!checkDeviceExtensionSupport(device, requiredExtensions)) {
            continue;
        }

        if (!checkSwapchainSupport(device)) {
            continue;
        }

        if (mSurface != VK_NULL_HANDLE) {
            QueueFamilyIndices indices = findQueueFamilies(device, mSurface);
            if (!indices.isComplete()) {
                continue;
            }
        }

        // Device supports all required extensions and queue families
        return device;
    }

    return VK_NULL_HANDLE;
    //throw std::runtime_error("failed to find a suitable GPU!");
}

// create the mDevice
void VulkanManager::createLogicalDevice(const std::vector<const char*>& requiredExtensions) {
    if (mSurface == VK_NULL_HANDLE) {
        throw std::runtime_error("Surface must be created before logical device");
    }
    QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice, mSurface);

    if (!indices.graphicsFamily.has_value() || !indices.presentFamily.has_value()) {
        throw std::runtime_error("Required queue families not found for logical device creation");
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies;
    if (indices.graphicsFamily.has_value()) {
        uniqueQueueFamilies.insert(indices.graphicsFamily.value());
    }
    if (indices.presentFamily.has_value()) {
        uniqueQueueFamilies.insert(indices.presentFamily.value());
    }

    // Add compute family if it's separate from graphics family
    if (indices.computeFamily.has_value() && indices.computeFamily != indices.graphicsFamily) {
        uniqueQueueFamilies.insert(indices.computeFamily.value());
    }

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }
    LOGI("Logical device created successfully.");
    checkDeviceProperties(mPhysicalDevice, mSurface);


    // Retrieve queues from the device
    // Note: We only have one queue for Android, it has both compute and graphics, but no presentation queue
    if (indices.graphicsFamily.has_value()) {
        vkGetDeviceQueue(mDevice, indices.graphicsFamily.value(), 0, &mGraphicsQueue);
    }

    if (indices.presentFamily.has_value()) {
        if (indices.presentFamily == indices.graphicsFamily) {
            mPresentQueue = mGraphicsQueue;
        } else {
            vkGetDeviceQueue(mDevice, indices.presentFamily.value(), 0, &mPresentQueue);
        }
    } else {
        mPresentQueue = mGraphicsQueue;
    }

    if (indices.computeFamily.has_value()) {
        if (indices.computeFamily == indices.graphicsFamily) {
            mComputeQueue = mGraphicsQueue;
        } else {
            vkGetDeviceQueue(mDevice, indices.computeFamily.value(), 0, &mComputeQueue);
        }
    } else {
        mComputeQueue = mGraphicsQueue;
    }
}

bool VulkanManager::checkSwapchainSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    bool hasSwapchainExtension = false;
    for (const auto& ext : availableExtensions) {
        if (strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            hasSwapchainExtension = true;
            break;
        }
    }
    if (!hasSwapchainExtension) {
        return false;
    }

    if (mSurface == VK_NULL_HANDLE) {
        return true;
    }

    SwapChainSupportDetails details = querySwapChainSupport(device, mSurface);
    return !details.formats.empty() && !details.presentModes.empty();
}

VulkanManager::SwapChainSupportDetails VulkanManager::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;

    // Query capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    LOGI("Format count before fetching: %d", formatCount);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        LOGI("Fetching formats returned: %d, actual formats fetched: %d", result, formatCount);
        for (const auto& format : details.formats) {
            LOGI("Format found: %d, Color Space: %d", format.format, format.colorSpace);
        }
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanManager::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR VulkanManager::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;  // Guaranteed to be available
}

VkExtent2D VulkanManager::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D actualExtent) {


    // return the extent from the native window
    return getWindowExtent();
    /*
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actual = { std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width)),
                              std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height)) };
        return actual;
    }*/
}

VkExtent2D VulkanManager::getWindowExtent() {
    int width = ANativeWindow_getWidth(mWindow);
    int height = ANativeWindow_getHeight(mWindow);

    return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
}

void VulkanManager::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(mPhysicalDevice, mSurface);

    if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) {
        LOGI("failed to find suitable swap chain details!");
        VkSurfaceFormatKHR fallbackFormat = {
                VK_FORMAT_B8G8R8A8_UNORM,
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        };
        swapChainSupport.formats.push_back(fallbackFormat);
    }

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);

    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, getWindowExtent());
    LOGI("Extent dimensions - width: %u, height: %u", extent.width, extent.height);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    LOGI("Initial image count: %u, Min count: %u, Max count: %u", imageCount, swapChainSupport.capabilities.minImageCount, swapChainSupport.capabilities.maxImageCount);

    // Clamp to double buffering explicitly
    if (swapChainSupport.capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, swapChainSupport.capabilities.maxImageCount);
        imageCount = std::min(imageCount, 2u);  // Clamp to two for double buffering
    } else {
        imageCount = std::min(imageCount, 2u);  // Assume no upper limit from maxImageCount
    }

    LOGI("Clamped image count to ensure double buffering: %u", imageCount);


    LOGI("Extents width %d height %d", extent.width, extent.height);

    // What in the configuration is causing VK_ERROR_FORMAT_NOT_SUPPORTED?

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = mSurface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice, mSurface);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value()/*, indices.presentFamily.value()*/};

    /*if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2; /////
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {

    }*/
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    // Retrieve the swap chain images
    vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, nullptr);
    mSwapChainImages.resize(imageCount);
    mSwapChainImageCount = imageCount;
    vkGetSwapchainImagesKHR(mDevice, mSwapChain, &mSwapChainImageCount, mSwapChainImages.data());
    // After retrieving images from the swapchain
    for (auto image : mSwapChainImages) {
        if (image == VK_NULL_HANDLE) {
            throw std::runtime_error("Found uninitialized image handle!");
        }
    }
    mSwapChainImageFormat = surfaceFormat.format;
    mSwapChainExtent = extent;

    mSwapChainImageViews.resize(imageCount);


    for (size_t i = 0; i < mSwapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = mSwapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = mSwapChainImageFormat;  // Format used in swapchain creation
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(mDevice, &createInfo, nullptr, &mSwapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image views!");
        }
    }
}


void VulkanManager::cleanupSwapChain() {
    for (auto framebuffer : mFramebuffers) {
        vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    }
    mFramebuffers.clear();

    for (auto imageView : mSwapChainImageViews) {
        vkDestroyImageView(mDevice, imageView, nullptr);
    }
    mSwapChainImageViews.clear();

    for (auto fence : mImagesInFlight) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(mDevice, fence, nullptr);
        }
    }
    mImagesInFlight.clear();

    vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
    mSwapChain = VK_NULL_HANDLE;

    // Also destroy and recreate graphics pipelines if they are dependent on the swapchain size
    vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
    mGraphicsPipeline = VK_NULL_HANDLE;
}

void VulkanManager::recreateSwapChain() {
    vkDeviceWaitIdle(mDevice);

    cleanupSwapChain();  // Function to destroy old swapchain and related resources

    createSwapChain();  // Recreate swapchain

    createFramebuffers();  // Recreate framebuffers if necessary

    // If your pipelines or other resources depend on the swapchain size, recreate them as well
    createGraphicsPipeline();  // Only if necessary
    // Update any necessary descriptors or buffers that depend on the swapchain size
    destroyShaderBuffers();
    createShaderBuffers();
    setupComputeDescriptorSet();
    initImagesInFlight();
}

/*createInfo.imageFormat and createInfo.imageColorSpace: The combination of image format and color space might not be supported by the Vulkan driver or hardware. You can check the supported combinations by calling vkGetPhysicalDeviceSurfaceFormatsKHR.
createInfo.imageExtent: The extent (width and height) of the swapchain images might not be supported. You can check the supported extent by examining the minImageExtent, maxImageExtent, and currentExtent fields of the VkSurfaceCapabilitiesKHR structure, which can be retrieved by calling vkGetPhysicalDeviceSurfaceCapabilitiesKHR.
createInfo.imageUsage: The usage flags of the swapchain images might not be supported. You can check the supported usage flags in the supportedUsageFlags field of the VkSurfaceCapabilitiesKHR structure.
createInfo.preTransform: The pre-transform might not be supported. You can check the supported transforms in the supportedTransforms field of the VkSurfaceCapabilitiesKHR structure.
createInfo.compositeAlpha: The composite alpha mode might not be supported. You can check the supported composite alpha modes in the supportedCompositeAlpha field of the VkSurfaceCapabilitiesKHR structure.
createInfo.presentMode: The present mode might not be supported. You can check the supported present modes by calling vkGetPhysicalDeviceSurfacePresentModesKHR.
*/
void VulkanManager::checkDeviceProperties(VkPhysicalDevice mPhysicalDevice, VkSurfaceKHR mSurface) {
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &capabilities);

    // Decode and log surface capabilities
    LOGI("Surface capabilities:");
    LOGI("Min image count: %u", capabilities.minImageCount);
    LOGI("Max image count: %u", capabilities.maxImageCount);
    LOGI("Current extent: width = %u, height = %u", capabilities.currentExtent.width, capabilities.currentExtent.height);
    LOGI("Min image extent: width = %u, height = %u", capabilities.minImageExtent.width, capabilities.minImageExtent.height);
    LOGI("Max image extent: width = %u, height = %u", capabilities.maxImageExtent.width, capabilities.maxImageExtent.height);
    LOGI("Max image array layers: %u", capabilities.maxImageArrayLayers);

    // Decode supported transforms
    std::string supportedTransforms = decodeSurfaceTransformFlags(capabilities.supportedTransforms);
    LOGI("Supported transforms: %s", supportedTransforms.c_str());
    LOGI("Current transform: %s", decodeSurfaceTransformFlags(capabilities.currentTransform).c_str());

    // Decode supported composite alpha
    std::string compositeAlphaFlags = decodeCompositeAlphaFlags(capabilities.supportedCompositeAlpha);
    LOGI("Supported composite alpha: %s", compositeAlphaFlags.c_str());

    // Decode supported usage flags
    std::string usageFlags = decodeUsageFlags(capabilities.supportedUsageFlags);
    LOGI("Supported usage flags: %s", usageFlags.c_str());

    // Query supported surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, surfaceFormats.data());

    LOGI("Supported surface formats:");
    for (const auto& format : surfaceFormats) {
        LOGI("Format: %u, Color space: %u", format.format, format.colorSpace);
    }

    // Query supported present modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &presentModeCount, presentModes.data());

    LOGI("Supported present modes:");
    for (const auto& mode : presentModes) {
        LOGI("Present mode: %u", mode);
    }
}

// Helper functions to decode Vulkan flags into strings
std::string VulkanManager::decodeSurfaceTransformFlags(VkSurfaceTransformFlagsKHR flags) {
    std::string description;
    if (flags & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) description += "IDENTITY, ";
    if (flags & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR) description += "ROTATE_90, ";
    if (flags & VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR) description += "ROTATE_180, ";
    if (flags & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) description += "ROTATE_270, ";
    if (flags & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR) description += "HORIZONTAL_MIRROR, ";
    if (flags & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR) description += "HORIZONTAL_MIRROR_ROTATE_90, ";
    if (flags & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR) description += "HORIZONTAL_MIRROR_ROTATE_180, ";
    if (flags & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR) description += "HORIZONTAL_MIRROR_ROTATE_270, ";
    if (flags & VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR) description += "INHERIT, ";
    if (description.empty()) description = "None";
    else description.pop_back(); // Remove trailing comma
    return description;
}

std::string VulkanManager::decodeCompositeAlphaFlags(VkCompositeAlphaFlagsKHR flags) {
    std::string description;
    if (flags & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) description += "OPAQUE, ";
    if (flags & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) description += "PRE_MULTIPLIED, ";
    if (flags & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) description += "POST_MULTIPLIED, ";
    if (flags & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) description += "INHERIT, ";
    if (description.empty()) description = "None";
    else description.pop_back(); // Remove trailing comma
    return description;
}

std::string VulkanManager::decodeUsageFlags(VkImageUsageFlags flags) {
    std::string description;
    if (flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) description += "TRANSFER_SRC, ";
    if (flags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) description += "TRANSFER_DST, ";
    if (flags & VK_IMAGE_USAGE_SAMPLED_BIT) description += "SAMPLED, ";
    if (flags & VK_IMAGE_USAGE_STORAGE_BIT) description += "STORAGE, ";
    if (flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) description += "COLOR_ATTACHMENT, ";
    if (flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) description += "DEPTH_STENCIL_ATTACHMENT, ";
    if (flags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) description += "TRANSIENT_ATTACHMENT, ";
    if (flags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) description += "INPUT_ATTACHMENT, ";
    if (description.empty()) description = "None";
    else description.pop_back(); // Remove trailing comma
    return description;
}



VulkanManager::QueueFamilyIndices VulkanManager::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    LOGI("Checking %d queue families.", queueFamilyCount);

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        LOGI("Queue Family #%d: Flags=0x%X", i, queueFamily.queueFlags);

        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
            LOGI("Graphics queue found at index %d.", i);

            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                indices.computeFamily = i;
                LOGI("Compute queue found at index %d.", i);
            }
            if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                indices.transferFamily = i;
                LOGI("Transfer queue found at index %d.", i);
            }
            if (queueFamily.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) {
                indices.sparseBindingFamily = i;
                LOGI("Sparse binding queue found at index %d.", i);
            }
        }

        if (surface != VK_NULL_HANDLE) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
                LOGI("Present queue found at index %d.", i);
            }
        }

        i++;
    }

    if (!indices.isComplete()) {
        LOGI("Not all required queue families were found.");
    }

    return indices;
}


void VulkanManager::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = mSwapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void VulkanManager::createGraphicsPipeline() {
    std::vector<char> vertShaderCode;
    std::vector<char> fragShaderCode;
    try {
        vertShaderCode = readFile("shaders/vertex_shader.spv");
        fragShaderCode = readFile("shaders/fragment_shader.spv");
    } catch (const std::exception& e) {
        LOGE("Failed to load shader binaries: %s", e.what());
        mGraphicsPipeline = VK_NULL_HANDLE;
        return;
    }

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescription = getSimpleBindingDescription();
    auto attributeDescriptions = getSimpleAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(mSwapChainExtent.width);
    viewport.height = static_cast<float>(mSwapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = mSwapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = mGraphicsPipelineLayout;
    pipelineInfo.renderPass = mRenderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mGraphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);
    vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
}


void VulkanManager::createComputePipeline() {
    // Read SPIR-V code from file
    std::vector<char> compShaderCode;
    try {
        compShaderCode = readFile("shaders/compute_shader.spv");
    } catch (const std::exception& e) {
        LOGE("Failed to load compute shader: %s", e.what());
        mComputePipeline = VK_NULL_HANDLE;
        return;
    }

    // Create shader module
    VkShaderModule compShaderModule = createShaderModule(compShaderCode);

    // Shader stage setup for compute shader
    VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
    compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compShaderStageInfo.module = compShaderModule;
    compShaderStageInfo.pName = "main";

    // Compute pipeline creation, using the created shader stage and pipeline layout
    VkComputePipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stage = compShaderStageInfo;
    pipelineCreateInfo.layout = mComputePipelineLayout;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // Not deriving from an existing pipeline
    pipelineCreateInfo.basePipelineIndex = -1; // Not deriving from an existing pipeline

    if (vkCreateComputePipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &mComputePipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline!");
    }

    // Cleanup
    vkDestroyShaderModule(mDevice, compShaderModule, nullptr);
}

void VulkanManager::createPipelineLayout() {
    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
    for (uint32_t i = 0; i < bindings.size(); ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[i].pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    descriptorLayoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(mDevice, &descriptorLayoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstantData);

    VkPipelineLayoutCreateInfo computeLayoutInfo{};
    computeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayoutInfo.setLayoutCount = 1;
    computeLayoutInfo.pSetLayouts = &mDescriptorSetLayout;
    computeLayoutInfo.pushConstantRangeCount = 1;
    computeLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(mDevice, &computeLayoutInfo, nullptr, &mComputePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline layout!");
    }

    VkPipelineLayoutCreateInfo graphicsLayoutInfo{};
    graphicsLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    graphicsLayoutInfo.setLayoutCount = 0;
    graphicsLayoutInfo.pSetLayouts = nullptr;
    graphicsLayoutInfo.pushConstantRangeCount = 0;
    graphicsLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(mDevice, &graphicsLayoutInfo, nullptr, &mGraphicsPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline layout!");
    }
}


// this to be called from createComputePipeline() at the end of the function.
void VulkanManager::setupComputeDescriptorSet() {
    if (mComputePipeline == VK_NULL_HANDLE) {
        return;
    }
    if (mDescriptorPool == VK_NULL_HANDLE) {
        throw std::runtime_error("descriptor pool not initialized before setting up compute descriptor set");
    }
    VkResult resetResult = vkResetDescriptorPool(mDevice, mDescriptorPool, 0);
    if (resetResult != VK_SUCCESS) {
        throw std::runtime_error("failed to reset descriptor pool");
    }
    // Allocate descriptor set first
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mDescriptorPool;  // Make sure you've created this
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &mDescriptorSetLayout;

    if (vkAllocateDescriptorSets(mDevice, &allocInfo, &mDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    // Assume buffers are created and named mVelocityBuffer, mPressureBuffer, etc.
    std::array<VkDescriptorBufferInfo, 4> bufferInfos{};
    bufferInfos[0] = {mVelocityBuffer, 0, VK_WHOLE_SIZE};
    bufferInfos[1] = {mPressureBuffer, 0, VK_WHOLE_SIZE};
    bufferInfos[2] = {mVelocityOutputBuffer, 0, VK_WHOLE_SIZE};
    bufferInfos[3] = {mPressureOutputBuffer, 0, VK_WHOLE_SIZE};

    std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

    for (size_t i = 0; i < bufferInfos.size(); ++i) {
        descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[i].dstSet = mDescriptorSet;
        descriptorWrites[i].dstBinding = static_cast<uint32_t>(i);
        descriptorWrites[i].dstArrayElement = 0;
        descriptorWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[i].descriptorCount = 1;
        descriptorWrites[i].pBufferInfo = &bufferInfos[i];
    }

    vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VulkanManager::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 1> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 4;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}


void VulkanManager::createCommandBufferForCompute() {
    // Create the Command Pool
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(mPhysicalDevice, mSurface);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    uint32_t computeFamilyIndex = queueFamilyIndices.computeFamily.value_or(queueFamilyIndices.graphicsFamily.value());
    poolInfo.queueFamilyIndex = computeFamilyIndex;  // Using computeFamily for compute commands

    //VkCommandPool computeCommandPool;
    vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mComputeCommandPool); // Create the compute command pool

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = mComputeCommandPool;  // Using the newly created compute command pool
    allocInfo.commandBufferCount = 1;

    //VkCommandBuffer mComputeCommandBuffer;
    vkAllocateCommandBuffers(mDevice, &allocInfo, &mComputeCommandBuffer);

    // Set up the command buffer begin information
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;  // Optional: Specify usage flags depending on how you plan to use this command buffer

    if (mComputePipeline != VK_NULL_HANDLE && mDescriptorSet != VK_NULL_HANDLE) {
        vkBeginCommandBuffer(mComputeCommandBuffer, &beginInfo);  // Begin recording the command buffer
        recordComputeOperations(mComputeCommandBuffer);

        vkEndCommandBuffer(mComputeCommandBuffer); // End recording the command buffer
    }
}

void VulkanManager::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(mPhysicalDevice, mSurface);
    if (!queueFamilyIndices.graphicsFamily.has_value()) {
        throw std::runtime_error("graphics queue family not available when creating command pool");
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics command pool!");
    }
}

void VulkanManager::createCommandBuffers() {
    if (mCommandPool == VK_NULL_HANDLE) {
        throw std::runtime_error("command pool must be created before allocating command buffers");
    }

    mCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = mCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(mCommandBuffers.size());

    if (vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

VkShaderModule VulkanManager::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(mDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

std::vector<char> VulkanManager::readFile(const std::string& filename) {
    if (mAssetManager != nullptr) {
        AAsset* asset = AAssetManager_open(mAssetManager, filename.c_str(), AASSET_MODE_BUFFER);
        if (asset != nullptr) {
            size_t length = AAsset_getLength(asset);
            std::vector<char> buffer(length);
            AAsset_read(asset, buffer.data(), length);
            AAsset_close(asset);
            return buffer;
        }
    }

    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    return buffer;
}

void VulkanManager::createSharedTexture() {
    VkExtent2D extent = getWindowExtent();
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = extent.width;  // Width of your simulation grid
    imageInfo.extent.height = extent.height; // Height of your simulation grid
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R32_SFLOAT;  // Single float to store density
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // Storage for compute, sampled for fragment
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(mDevice, &imageInfo, nullptr, &mTextureImage) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(mDevice, mTextureImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(mDevice, &allocInfo, nullptr, &mTextureImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }
    vkBindImageMemory(mDevice, mTextureImage, mTextureImageMemory, 0);
}

uint32_t VulkanManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void VulkanManager::initSynchronization() {
    mInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    mImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    mRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
}

void VulkanManager::initVulkanFences() {
    mInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start with fences signaled

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization fences for frame " + std::to_string(i));
        }
    }
}

void VulkanManager::initSemaphores() {
    mImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    mRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    mComputeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mComputeFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create semaphores for frame " + std::to_string(i));
        }
    }
}

void VulkanManager::initImagesInFlight() {
    mImagesInFlight.resize(mSwapChainImageCount, VK_NULL_HANDLE);  // Initialize with VK_NULL_HANDLE indicating no fence is associated initially.

    // mSwapChainImages and mInFlightFences are already initialized
    for (size_t i = 0; i < mSwapChainImageCount; ++i) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start with fences signaled

        VkFence fence;
        if (vkCreateFence(mDevice, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create fence for image in flight");
        }

        mImagesInFlight[i] = fence;  // Store the fence
    }
}

void VulkanManager::recordComputeOperations(VkCommandBuffer commandBuffer) {
    if (mComputePipeline == VK_NULL_HANDLE) {
        return;
    }
    // Bind the compute pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mComputePipeline);

    // Bind descriptor sets for compute shader
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mComputePipelineLayout, 0, 1, &mDescriptorSet, 0, nullptr);

    // Dispatch the compute operations, you need to define how many groups to dispatch
    uint32_t groupCountX = (mSwapChainExtent.width + 15) / 16;  // Assuming each group handles a 16x16 block
    uint32_t groupCountY = (mSwapChainExtent.height + 15) / 16;
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         1,
                         &memoryBarrier,
                         0,
                         nullptr,
                         0,
                         nullptr);
}

void VulkanManager::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    // Command buffer begin info
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // Start the render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = mRenderPass;
    renderPassInfo.framebuffer = mFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = mSwapChainExtent;

    VkClearValue clearColor = {{0.0f, 0.0f, 0.0f, 1.0f}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the graphics pipeline
    if (mGraphicsPipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

        if (mVertexBuffer != VK_NULL_HANDLE && mVertexCount > 0) {
            VkBuffer vertexBuffers[] = {mVertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdDraw(commandBuffer, mVertexCount, 1, 0, 0);
        }
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}

void VulkanManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                 VkMemoryPropertyFlags properties, VkBuffer& buffer,
                                 VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(mDevice, &bufferInfo, nullptr,
                       &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(mDevice, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(mDevice, &allocInfo, nullptr,
                         &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(mDevice, buffer, bufferMemory, 0);
}

void VulkanManager::createFramebuffers() {
    mFramebuffers.resize(mSwapChainImageViews.size());

    for (size_t i = 0; i < mSwapChainImageViews.size(); i++) {
        VkImageView attachments[] = {
                mSwapChainImageViews[i]  // Assuming a single attachment per framebuffer
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = mRenderPass;  // Already created render pass
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = mSwapChainExtent.width;
        framebufferInfo.height = mSwapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create a framebuffer!");
        }
    }
}

void VulkanManager::createShaderBuffers() {
    // todo: refactor mSwapChainExtent to be mWindowExtent.
    VkDeviceSize velocitySize = mSwapChainExtent.width * mSwapChainExtent.height * sizeof(float) * 2; // vec2 for each pixel
    VkDeviceSize pressureSize = mSwapChainExtent.width * mSwapChainExtent.height * sizeof(float); // float for each pixel

    // Create velocity buffer
    createBuffer(velocitySize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mVelocityBuffer, mVelocityBufferMemory);

    // Create pressure buffer
    createBuffer(pressureSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mPressureBuffer, mPressureBufferMemory);

    // Create velocity output buffer
    createBuffer(velocitySize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mVelocityOutputBuffer, mVelocityOutputBufferMemory);

    // Create pressure output buffer
    createBuffer(pressureSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mPressureOutputBuffer, mPressureOutputBufferMemory);
}

void VulkanManager::destroyShaderBuffers() {
    if (mVelocityBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(mDevice, mVelocityBuffer, nullptr);
        mVelocityBuffer = VK_NULL_HANDLE;
    }
    if (mVelocityBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mDevice, mVelocityBufferMemory, nullptr);
        mVelocityBufferMemory = VK_NULL_HANDLE;
    }
    if (mPressureBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(mDevice, mPressureBuffer, nullptr);
        mPressureBuffer = VK_NULL_HANDLE;
    }
    if (mPressureBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mDevice, mPressureBufferMemory, nullptr);
        mPressureBufferMemory = VK_NULL_HANDLE;
    }
    if (mVelocityOutputBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(mDevice, mVelocityOutputBuffer, nullptr);
        mVelocityOutputBuffer = VK_NULL_HANDLE;
    }
    if (mVelocityOutputBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mDevice, mVelocityOutputBufferMemory, nullptr);
        mVelocityOutputBufferMemory = VK_NULL_HANDLE;
    }
    if (mPressureOutputBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(mDevice, mPressureOutputBuffer, nullptr);
        mPressureOutputBuffer = VK_NULL_HANDLE;
    }
    if (mPressureOutputBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mDevice, mPressureOutputBufferMemory, nullptr);
        mPressureOutputBufferMemory = VK_NULL_HANDLE;
    }
    mDescriptorSet = VK_NULL_HANDLE;
}

void VulkanManager::createVertexBuffer() {
    if (mVertexBuffer != VK_NULL_HANDLE) {
        return;
    }

    const std::array<SimpleVertex, 3> vertices = {
            SimpleVertex{{-1.0f, -1.0f}, {0.0f, 0.0f}},
            SimpleVertex{{3.0f, -1.0f}, {2.0f, 0.0f}},
            SimpleVertex{{-1.0f, 3.0f}, {0.0f, 2.0f}},
    };

    VkDeviceSize bufferSize = sizeof(vertices);
    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 mVertexBuffer,
                 mVertexBufferMemory);

    void* data = nullptr;
    VkResult mapResult = vkMapMemory(mDevice, mVertexBufferMemory, 0, bufferSize, 0, &data);
    if (mapResult != VK_SUCCESS) {
        throw std::runtime_error("failed to map vertex buffer memory");
    }
    std::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(mDevice, mVertexBufferMemory);
    mVertexCount = static_cast<uint32_t>(vertices.size());
}

void VulkanManager::destroyVertexBuffer() {
    if (mVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(mDevice, mVertexBuffer, nullptr);
        mVertexBuffer = VK_NULL_HANDLE;
    }
    if (mVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mDevice, mVertexBufferMemory, nullptr);
        mVertexBufferMemory = VK_NULL_HANDLE;
    }
    mVertexCount = 0;
}

// Let's let JNI call this so the app can pause and resume, lifecycle etc.
void VulkanManager::drawFrame(float delta, float x, float y, bool isTouching) {
    static int currentFrame = 0;

    LOGI("x=%f y=%f ",x,y);
    // Wait for the previous frame to finish
    vkWaitForFences(mDevice, 1, &mInFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(mDevice, 1, &mInFlightFences[currentFrame]);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(mDevice, mSwapChain, UINT64_MAX, mImageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // Handle swapchain recreation
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    if (mImagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(mDevice, 1, &mImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    mImagesInFlight[imageIndex] = mInFlightFences[currentFrame];

    // Prepare for compute operations
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    bool hasCompute = (mComputePipeline != VK_NULL_HANDLE) && (mDescriptorSet != VK_NULL_HANDLE);
    if (hasCompute && mComputeQueue == VK_NULL_HANDLE) {
        hasCompute = false;
    }
    if (hasCompute) {
        vkResetCommandBuffer(mComputeCommandBuffer, 0);
        vkBeginCommandBuffer(mComputeCommandBuffer, &beginInfo);

        PushConstantData pcData{delta, 0.1f, static_cast<int>(mSwapChainExtent.width), static_cast<int>(mSwapChainExtent.height), glm::vec2(x, y), isTouching ? 1 : 0};
        vkCmdPushConstants(mComputeCommandBuffer, mComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantData), &pcData);
        recordComputeOperations(mComputeCommandBuffer);
        vkEndCommandBuffer(mComputeCommandBuffer);

        VkSubmitInfo computeSubmitInfo{};
        computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        computeSubmitInfo.commandBufferCount = 1;
        computeSubmitInfo.pCommandBuffers = &mComputeCommandBuffer;
        computeSubmitInfo.signalSemaphoreCount = 1;
        VkSemaphore computeSignals[] = {mComputeFinishedSemaphores[currentFrame]};
        computeSubmitInfo.pSignalSemaphores = computeSignals;
        vkQueueSubmit(mComputeQueue, 1, &computeSubmitInfo, VK_NULL_HANDLE);
    }

    // Graphics queue submission
    vkResetCommandBuffer(mCommandBuffers[currentFrame], 0);
    recordCommandBuffer(mCommandBuffers[currentFrame], imageIndex);

    VkSubmitInfo graphicsSubmitInfo{};
    graphicsSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[2];
    VkPipelineStageFlags waitStages[2];
    uint32_t waitCount = 0;
    waitSemaphores[waitCount] = mImageAvailableSemaphores[currentFrame];
    waitStages[waitCount] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    waitCount++;
    if (hasCompute) {
        waitSemaphores[waitCount] = mComputeFinishedSemaphores[currentFrame];
        waitStages[waitCount] = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        waitCount++;
    }
    graphicsSubmitInfo.waitSemaphoreCount = waitCount;
    graphicsSubmitInfo.pWaitSemaphores = waitSemaphores;
    graphicsSubmitInfo.pWaitDstStageMask = waitStages;
    graphicsSubmitInfo.commandBufferCount = 1;
    graphicsSubmitInfo.pCommandBuffers = &mCommandBuffers[currentFrame];
    VkSemaphore signalSemaphores[] = {mRenderFinishedSemaphores[currentFrame]};
    graphicsSubmitInfo.signalSemaphoreCount = 1;
    graphicsSubmitInfo.pSignalSemaphores = signalSemaphores;
    vkQueueSubmit(mGraphicsQueue, 1, &graphicsSubmitInfo, mInFlightFences[currentFrame]);

    // Presenting the image
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR swapChains[] = {mSwapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    vkQueuePresentKHR(mPresentQueue, &presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}


void VulkanManager::cleanup() {
    if (mDevice != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(mDevice);
    }

    for (auto fence : mImagesInFlight) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(mDevice, fence, nullptr);
        }
    }
    mImagesInFlight.clear();

    for (auto fence : mInFlightFences) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(mDevice, fence, nullptr);
        }
    }
    mInFlightFences.clear();

    for (size_t i = 0; i < mImageAvailableSemaphores.size(); ++i) {
        if (mImageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
        }
        if (mRenderFinishedSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
        }
        if (i < mComputeFinishedSemaphores.size() && mComputeFinishedSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(mDevice, mComputeFinishedSemaphores[i], nullptr);
        }
    }
    mImageAvailableSemaphores.clear();
    mRenderFinishedSemaphores.clear();
    mComputeFinishedSemaphores.clear();

    for (auto framebuffer : mFramebuffers) {
        vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    }
    mFramebuffers.clear();

    if (mGraphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
        mGraphicsPipeline = VK_NULL_HANDLE;
    }
    if (mGraphicsPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(mDevice, mGraphicsPipelineLayout, nullptr);
        mGraphicsPipelineLayout = VK_NULL_HANDLE;
    }
    if (mComputePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(mDevice, mComputePipeline, nullptr);
        mComputePipeline = VK_NULL_HANDLE;
    }
    if (mComputePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(mDevice, mComputePipelineLayout, nullptr);
        mComputePipelineLayout = VK_NULL_HANDLE;
    }
    if (mDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
        mDescriptorPool = VK_NULL_HANDLE;
    }
    if (mDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);
        mDescriptorSetLayout = VK_NULL_HANDLE;
    }
    mDescriptorSet = VK_NULL_HANDLE;

    if (mRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        mRenderPass = VK_NULL_HANDLE;
    }

    destroyVertexBuffer();

    if (mCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
        mCommandPool = VK_NULL_HANDLE;
    }
    if (mComputeCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(mDevice, mComputeCommandPool, nullptr);
        mComputeCommandPool = VK_NULL_HANDLE;
    }

    for (auto imageView : mSwapChainImageViews) {
        vkDestroyImageView(mDevice, imageView, nullptr);
    }
    mSwapChainImageViews.clear();

    if (mSwapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
        mSwapChain = VK_NULL_HANDLE;
    }

    if (mVelocityBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(mDevice, mVelocityBuffer, nullptr);
        mVelocityBuffer = VK_NULL_HANDLE;
    }
    if (mVelocityBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mDevice, mVelocityBufferMemory, nullptr);
        mVelocityBufferMemory = VK_NULL_HANDLE;
    }
    if (mPressureBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(mDevice, mPressureBuffer, nullptr);
        mPressureBuffer = VK_NULL_HANDLE;
    }
    if (mPressureBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mDevice, mPressureBufferMemory, nullptr);
        mPressureBufferMemory = VK_NULL_HANDLE;
    }
    if (mVelocityOutputBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(mDevice, mVelocityOutputBuffer, nullptr);
        mVelocityOutputBuffer = VK_NULL_HANDLE;
    }
    if (mVelocityOutputBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mDevice, mVelocityOutputBufferMemory, nullptr);
        mVelocityOutputBufferMemory = VK_NULL_HANDLE;
    }
    if (mPressureOutputBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(mDevice, mPressureOutputBuffer, nullptr);
        mPressureOutputBuffer = VK_NULL_HANDLE;
    }
    if (mPressureOutputBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mDevice, mPressureOutputBufferMemory, nullptr);
        mPressureOutputBufferMemory = VK_NULL_HANDLE;
    }

    if (mTextureImage != VK_NULL_HANDLE) {
        vkDestroyImage(mDevice, mTextureImage, nullptr);
        mTextureImage = VK_NULL_HANDLE;
    }
    if (mTextureImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mDevice, mTextureImageMemory, nullptr);
        mTextureImageMemory = VK_NULL_HANDLE;
    }

    if (mDevice != VK_NULL_HANDLE) {
        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
    }

    if (mInstance != VK_NULL_HANDLE && mSurface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        mSurface = VK_NULL_HANDLE;
    }

    if (mInstance != VK_NULL_HANDLE) {
        vkDestroyInstance(mInstance, nullptr);
        mInstance = VK_NULL_HANDLE;
    }

    if (mWindow) {
        ANativeWindow_release(mWindow);
        mWindow = nullptr;
    }

    JNIEnv* env;
    if (mJvm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
        if (mActivity != nullptr) {
            env->DeleteGlobalRef(mActivity);
            mActivity = nullptr;
        }
        mJvm->DetachCurrentThread();
    }
}

void VulkanManager::notifyClient() {
    JNIEnv* env = nullptr;
    // Attach the current thread to the JVM to obtain a valid JNIEnv pointer
    if (mJvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
        return; // Failed to attach the thread
    }

    // Get the MainActivity class
    jclass clazz = env->FindClass("com/aniviza/fingersmoke20/MainActivity");
    if (clazz == nullptr) {
        mJvm->DetachCurrentThread();
        return; // Class not found
    }

    // Get the ID of the method that starts the rendering loop
    jmethodID methodId = env->GetMethodID(clazz, "startRenderLoop", "()V");
    if (methodId == nullptr) {
        mJvm->DetachCurrentThread();
        return; // Method not found
    }

    // Find the global reference to the MainActivity object
    // Assuming 'mainActivityObj' is globally stored during initialization
    if (mActivity == nullptr) {
        mJvm->DetachCurrentThread();
        return; // Object reference not found
    }

    // Call the Java method
    env->CallVoidMethod(mActivity, methodId);

    // Clean up and detach from the thread
    mJvm->DetachCurrentThread();
}
/*
void VulkanManager::updateTouch(float x, float y, bool isTouching) {
    PushConstantData pcData{};
    pcData.deltaTime = calculateDeltaTime(); // Implement this based on your timing logic
    pcData.visc = 0.1f; // Example viscosity value
    pcData.width = mSwapChainExtent.width;
    pcData.height = mSwapChainExtent.height;
    pcData.touchPos = glm::vec2(x, y);
    pcData.isTouching = isTouching;
    vkCmdPushConstants(mComputeCommandBuffer, mComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(touch), touch);
}
*/

// JNI

static JavaVM* jvm;
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    jvm = vm;
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aniviza_fingersmoke20_MainActivity_initVulkan(JNIEnv* env, jobject mainActivity, jobject surface) {
    jobject globalActivityRef = env->NewGlobalRef(mainActivity);  // Create a global reference to the MainActivity object

    jobject globalSurface = env->NewGlobalRef(surface); // Create a global reference to keep the surface

    std::thread initThread([globalActivityRef, globalSurface]() {
        JNIEnv* newEnv;
        jvm->AttachCurrentThread(&newEnv, nullptr); // Attach the thread to get a valid JNIEnv

        ANativeWindow *window = ANativeWindow_fromSurface(newEnv, globalSurface);
        jclass activityClass = newEnv->GetObjectClass(globalActivityRef);
        jmethodID getAssets = newEnv->GetMethodID(activityClass, "getAssets", "()Landroid/content/res/AssetManager;");
        jobject assetManagerObj = newEnv->CallObjectMethod(globalActivityRef, getAssets);
        AAssetManager* assetManager = nullptr;
        if (assetManagerObj != nullptr) {
            assetManager = AAssetManager_fromJava(newEnv, assetManagerObj);
        }
        newEnv->DeleteLocalRef(activityClass);
        if (assetManagerObj != nullptr) {
            newEnv->DeleteLocalRef(assetManagerObj);
        }
        if (vkManager == nullptr) {
            vkManager = new VulkanManager(jvm,globalActivityRef,window, assetManager); // Initialize Vulkan
            vkManager->initVulkan();
        }

        newEnv->DeleteGlobalRef(globalSurface); // Cleanup global reference
        jvm->DetachCurrentThread(); // Detach thread when done
    });
    initThread.detach(); // Detach the thread to avoid having to join it later
}

extern "C" JNIEXPORT void JNICALL
Java_com_aniviza_fingersmoke20_MainActivity_drawFrame(JNIEnv* env, jobject obj, jfloat delta, jfloat x, jfloat y, jboolean isTouching) {
    if (vkManager != nullptr) {
        vkManager->drawFrame(delta, x, y, isTouching);
    }
}
extern "C" JNIEXPORT void JNICALL
Java_com_aniviza_fingersmoke20_MainActivity_cleanup(JNIEnv*, jobject) {
    if (vkManager != nullptr) {
        delete vkManager;
        vkManager = nullptr;  // Reset the pointer after deletion to avoid dangling pointer issues
    }
}


