//
// Created by rick on 4/20/24.
//

#include "fs20.h"


VulkanManager::VulkanManager(JavaVM* jvm, jobject globalActivityRef, ANativeWindow *window)
        : mJvm(jvm), mActivity(globalActivityRef), mWindow(window), mInstance(VK_NULL_HANDLE) {}

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
            //VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            //VK_KHR_SURFACE_EXTENSION_NAME,
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

    VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;

    selectedDevice = pickSuitableDevice(devices, requiredExtensions);
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

    // create mDevice
    createLogicalDevice(requiredExtensions);

    // Create the Android Surface
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.window = mWindow;  // ANativeWindow pointer obtained from Android environment.

    if (vkCreateAndroidSurfaceKHR(mInstance, &surfaceCreateInfo, nullptr,
                                  &mSurface) != VK_SUCCESS) {
        std::cerr << "Failed to create Android surface!" << std::endl;
        return -1;
    }

    // Check if the surface is supported by the physical device
    VkBool32 surfaceSupported = VK_FALSE;
    result = vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, 0, mSurface, &surfaceSupported);
    if (result != VK_SUCCESS || !surfaceSupported) {
        LOGE("Surface is not supported by the physical device: %d", result);
        return -1;
    }

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

    createGraphicsPipeline();
    createPipelineLayout();
    createComputePipeline();
    createSharedTexture();
    initVulkanFences();
    initSynchronization();
    initSemaphores();
    initImagesInFlight();
    createFramebuffers();
    createShaderBuffers();

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
            return VK_SUCCESS;
        }
    }
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
        if (checkDeviceExtensionSupport(device, requiredExtensions)) {
            // Device supports all required extensions
            return device;
        }
    }

    return VK_NULL_HANDLE;
    //throw std::runtime_error("failed to find a suitable GPU!");
}

// create the mDevice
void VulkanManager::createLogicalDevice(const std::vector<const char*>& requiredExtensions) {
    QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice, mSurface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(),
            //indices.presentFamily.value()
    };

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
    vkGetDeviceQueue(mDevice, indices.graphicsFamily.value(), 0, &mGraphicsQueue);
    //if (indices.presentFamily.value() == indices.graphicsFamily.value()) {
        mPresentQueue = mGraphicsQueue;  // Same queue for graphics and presentation
    //} else {
    //    vkGetDeviceQueue(mDevice, indices.presentFamily.value(), 0, &mPresentQueue);
    //}
}

bool VulkanManager::checkSwapchainSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    for (const auto& ext : availableExtensions) {
        if (strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            return true;
        }
    }
    return false;
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

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
            LOGI("Present queue found at index %d.", i);
        }

        i++;
    }

    if (!indices.isComplete()) {
        LOGI("Not all required queue families were found.");
    }

    return indices;
}


void VulkanManager::createGraphicsPipeline() {
    // Assuming shaders are precompiled to SPIR-V and stored as .spv files in cmake
    auto vertShaderCode = readFile("shaders/vertex_shader.spv");
    auto fragShaderCode = readFile("shaders/fragment_shader.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    // Define shader stage create info for vertex and fragment shaders
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // Define the pipeline's fixed-function stages (e.g., input assembly, viewport, rasterization)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    std::vector<VkVertexInputBindingDescription> bindingDescriptions = Vertex::getBindingDescriptions();
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions = Vertex::getAttributeDescriptions();
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Configure input assembly based on your application's needs
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor rectangle
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(mSwapChainExtent.width);
    viewport.height = static_cast<float>(mSwapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = mSwapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Further configuration for pipeline creation (omitted)
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    VkDescriptorSetLayout descriptorSetLayout;
    vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &descriptorSetLayout);


    // Now create the pipeline layout that uses this descriptor set layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1; // Optional
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    // Define the render pass
    // Setup for a simple render pass with one color attachment
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


    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    // Include other pipeline states
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = mRenderPass;


    if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mGraphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    // Clean up temporary objects
    vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);
    vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
}


void VulkanManager::createComputePipeline() {
    // Read SPIR-V code from file
    auto compShaderCode = readFile("shaders/compute_shader.spv");

    // Create shader module
    VkShaderModule compShaderModule = createShaderModule(compShaderCode);

    // Shader stage setup for compute shader
    VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
    compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compShaderStageInfo.module = compShaderModule;
    compShaderStageInfo.pName = "main";

    // Create pipeline layout
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = 0;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = 1;
    descriptorLayoutInfo.pBindings = &layoutBinding;

    VkDescriptorSetLayout descriptorSetLayout;
    if (vkCreateDescriptorSetLayout(mDevice, &descriptorLayoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    // Pipeline layout that uses this descriptor set layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1; // Use one descriptor set layout
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional: For passing small amounts of data
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mComputePipelineLayout);

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

    setupComputeDescriptorSet();

    // Cleanup
    vkDestroyShaderModule(mDevice, compShaderModule, nullptr);
}

void VulkanManager::createPipelineLayout() {
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = 0;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBinding.pImmutableSamplers = nullptr; // Not needed for storage buffers

    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = 1;
    descriptorLayoutInfo.pBindings = &layoutBinding;

    VkDescriptorSetLayout descriptorSetLayout;
    if (vkCreateDescriptorSetLayout(mDevice, &descriptorLayoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    // Define push constant range
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT; // Or combine flags for multiple stages
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstantData); // Make sure this size matches the structure in shaders

    // Create the pipeline layout that includes both descriptor set layouts and push constants
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1; // We have one descriptor set layout
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1; // We are using push constants
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mComputePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    // It's generally good practice to keep the descriptor set layout around if you will use it later
    // for creating descriptor sets, do not destroy it immediately after creating the pipeline layout
}


// this to be called from createComputePipeline() at the end of the function.
void VulkanManager::setupComputeDescriptorSet() {
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


void VulkanManager::createCommandBufferForCompute() {
    // Create the Command Pool
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(mPhysicalDevice, mSurface);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.computeFamily.value();  // Using computeFamily for compute commands

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

    vkBeginCommandBuffer(mComputeCommandBuffer, &beginInfo);  // Begin recording the command buffer
    vkCmdBindPipeline(mComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mComputePipeline);
    vkCmdBindDescriptorSets(mComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mComputePipelineLayout, 0, 1, &mDescriptorSet, 0, nullptr);

    // Specify the number of workgroups to dispatch in the compute shader
    vkCmdDispatch(mComputeCommandBuffer, (mSwapChainExtent.width + 15) / 16,
                  (mSwapChainExtent.height + 15) / 16, 1); // Assuming local_size_x = local_size_y = 16 in shader

    vkEndCommandBuffer(mComputeCommandBuffer); // End recording the command buffer

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
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = (size_t) file.tellg();
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

    VkDeviceMemory textureImageMemory;
    if (vkAllocateMemory(mDevice, &allocInfo, nullptr, &textureImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }
    vkBindImageMemory(mDevice, mTextureImage, textureImageMemory, 0);
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

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start all fences as signaled

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame");
        }
    }

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

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS) {
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

void VulkanManager::recordComputeOperations(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    // Bind the compute pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mComputePipeline);

    // Bind descriptor sets for compute shader
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mComputePipelineLayout, 0, 1, &mDescriptorSet, 0, nullptr);

    // Dispatch the compute operations, you need to define how many groups to dispatch
    uint32_t groupCountX = (mSwapChainExtent.width + 15) / 16;  // Assuming each group handles a 16x16 block
    uint32_t groupCountY = (mSwapChainExtent.height + 15) / 16;
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
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
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

    // Draw
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);  // Drawing a triangle without a vertex buffer

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

// Let's let JNI call this so the app can pause and resume, lifecycle etc.
void VulkanManager::drawFrame(float delta, float x, float y, bool isTouching) {
    static int currentFrame = 0;

    LOGI("x=%f y=%f ",x,y);
    // Wait for the previous frame to finish
    vkWaitForFences(mDevice, 1, &mInFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

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

    vkResetCommandBuffer(mComputeCommandBuffer, 0);
    vkBeginCommandBuffer(mComputeCommandBuffer, &beginInfo);

    PushConstantData pcData{delta, 0.1f, static_cast<int>(mSwapChainExtent.width), static_cast<int>(mSwapChainExtent.height), glm::vec2(x, y), isTouching};
    vkCmdPushConstants(mComputeCommandBuffer, mComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantData), &pcData);
    recordComputeOperations(mComputeCommandBuffer, imageIndex);
    vkEndCommandBuffer(mComputeCommandBuffer);

    VkSubmitInfo computeSubmitInfo{};
    computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    computeSubmitInfo.commandBufferCount = 1;
    computeSubmitInfo.pCommandBuffers = &mComputeCommandBuffer;
    vkQueueSubmit(mComputeQueue, 1, &computeSubmitInfo, mInFlightFences[currentFrame]);

    // Graphics queue submission
    vkResetCommandBuffer(mCommandBuffers[currentFrame], 0);
    recordCommandBuffer(mCommandBuffers[currentFrame], imageIndex);

    VkSubmitInfo graphicsSubmitInfo{};
    graphicsSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = {mRenderFinishedSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    graphicsSubmitInfo.waitSemaphoreCount = 1;
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
    if (mInstance != VK_NULL_HANDLE) {
        vkDestroyInstance(mInstance, nullptr);
        mInstance = VK_NULL_HANDLE;
    }
    // Clean up other Vulkan resources like device, swapchain, etc.
    ANativeWindow_release(mWindow);

    for (auto framebuffer : mFramebuffers) {
        vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    }
    mFramebuffers.clear();

    for (auto fence : mImagesInFlight) {
        vkDestroyFence(mDevice, fence, nullptr);
    }

    for (auto imageView : mSwapChainImageViews) {
        vkDestroyImageView(mDevice, imageView, nullptr);
    }
    mSwapChainImageViews.clear();

#ifdef USES_DEPTH_IMAGE_VIEW
    // If using depth resources, destroy them
    if (mDepthImageView) {
        vkDestroyImageView(mDevice, mDepthImageView, nullptr);
        vkDestroyImage(mDevice, mDepthImage, nullptr);
        vkFreeMemory(mDevice, mDepthImageMemory, nullptr);
    }
#endif

    // Finally, destroy the swapchain
    vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
    mSwapChain = VK_NULL_HANDLE;

    vkDestroyBuffer(mDevice, mVelocityBuffer, nullptr);
    vkFreeMemory(mDevice, mVelocityBufferMemory, nullptr);

    vkDestroyBuffer(mDevice, mPressureBuffer, nullptr);
    vkFreeMemory(mDevice, mPressureBufferMemory, nullptr);

    vkDestroyBuffer(mDevice, mVelocityOutputBuffer, nullptr);
    vkFreeMemory(mDevice, mVelocityOutputBufferMemory, nullptr);

    vkDestroyBuffer(mDevice, mPressureOutputBuffer, nullptr);
    vkFreeMemory(mDevice, mPressureOutputBufferMemory, nullptr);

    JNIEnv* env;
    mJvm->AttachCurrentThread(&env, nullptr);
    env->DeleteGlobalRef(mActivity);  // Clean up global reference
    mJvm->DetachCurrentThread();

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
        if (vkManager == nullptr) {
            vkManager = new VulkanManager(jvm,globalActivityRef,window); // Initialize Vulkan
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


