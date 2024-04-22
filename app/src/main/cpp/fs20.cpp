//
// Created by rick on 4/20/24.
//

#include "fs20.h"

VulkanManager::VulkanManager(ANativeWindow *window)
        : mWindow(window), mInstance(VK_NULL_HANDLE) {
    initVulkan();
}

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
    appInfo.pApplicationName = "Hello Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;  // Or whatever version you're targeting

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    const std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> extensions = {
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };

    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkResult result = vkCreateInstance(&createInfo, nullptr, &mInstance);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance!" << std::endl;
        return -1;
    }


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

    if (CreateDebugUtilsMessengerEXT(mInstance, &debugCreateInfo, nullptr, &mDebugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }

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

    std::vector<const char*> requiredExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };

    VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;

    selectedDevice = pickSuitableDevice(devices, requiredExtensions);
    if (selectedDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }

    mPhysicalDevice = selectedDevice;

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

    return 0;
}

std::vector<VkExtensionProperties> getAvailableExtensions(VkPhysicalDevice device) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr,
                                         &extensionCount, nullptr);  // Get the count

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());  // Fetch the extensions
    return extensions;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device,
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
        if (checkDeviceExtensionSupport(device, requiredExtensions)) {
            // Device supports all required extensions
            return device;
        }
    }

    throw std::runtime_error("failed to find a suitable GPU!");
}

// create the mDevice
void VulkanManager::createLogicalDevice(const std::vector<const char*>& requiredExtensions) {
    QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice, mSurface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

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

    // Retrieve queues from the device
    vkGetDeviceQueue(mDevice, indices.graphicsFamily.value(), 0, &mGraphicsQueue);
    if (indices.presentFamily.value() == indices.graphicsFamily.value()) {
        mPresentQueue = mGraphicsQueue;  // Same queue for graphics and presentation
    } else {
        vkGetDeviceQueue(mDevice, indices.presentFamily.value(), 0, &mPresentQueue);
    }
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
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
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
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actual = { std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width)),
                              std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height)) };
        return actual;
    }
}

VkExtent2D VulkanManager::getWindowExtent() {
    int width = ANativeWindow_getWidth(mWindow);
    int height = ANativeWindow_getHeight(mWindow);

    return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
}

void VulkanManager::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(mPhysicalDevice, mSurface);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, getWindowExtent());  // 'windowExtent' should be passed based on the window size

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

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
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

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


VulkanManager::QueueFamilyIndices VulkanManager::findQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        // Check for graphics capability
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            if (!indices.graphicsFamily.has_value()) {
                indices.graphicsFamily = i;
            }
        }

        // Check for presentation capability
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if (presentSupport && !indices.presentFamily.has_value()) {
            indices.presentFamily = i;
        }

        // Check for a separate compute capability
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            if (!indices.computeFamily.has_value() && !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                // Prefer a separate compute queue if available and not also a graphics queue
                indices.computeFamily = i;
            }
        }

        // Ensure all required queue families are found
        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    // If a separate compute queue isn't found, it can default to using the graphics queue
    if (!indices.computeFamily.has_value() && indices.graphicsFamily.has_value()) {
        indices.computeFamily = indices.graphicsFamily;
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

    // Now create the pipeline layout that uses this descriptor set layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1; // We have one descriptor set layout
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional: For push constants
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional: Push constants

    if (vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mComputePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    // vkDestroyDescriptorSetLayout(mDevice, descriptorSetLayout, nullptr); // Clean up on exit tbd.
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
void VulkanManager::drawFrame() {
    static int currentFrame = 0;
    // Wait for the previous frame to finish
    vkWaitForFences(mDevice, 1, &mInFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(mDevice, mSwapChain, UINT64_MAX, mImageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    // Check if a previous frame is using this image (i.e., there is its fence to wait on)
    if (mImagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(mDevice, 1, &mImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    // Mark the image as now being in use by this frame
    mImagesInFlight[imageIndex] = mInFlightFences[currentFrame];

    vkResetFences(mDevice, 1, &mInFlightFences[currentFrame]);

    vkResetCommandBuffer(mCommandBuffers[currentFrame], 0);
    recordCommandBuffer(mCommandBuffers[currentFrame], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {mImageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &mCommandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {mRenderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mInFlightFences[currentFrame]);

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


}

void VulkanManager::pushTouch(float x, float y) {
    float touch[2] = {x, y};
    vkCmdPushConstants(mComputeCommandBuffer, mComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(touch), touch);
}


// JNI

static JavaVM* jvm;
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    jvm = vm;
    return VK_SUCCESS;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_myapp_MyActivity_initVulkan(JNIEnv* env, jobject, jobject surface) {
    if (vkManager == nullptr) {
        ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
        vkManager = new VulkanManager(window); // will call initVulcan()
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_myapp_MyActivity_drawFrame(JNIEnv*, jobject) {
    if (vkManager != nullptr) {
        vkManager->drawFrame();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_myapp_MyActivity_cleanupVulkan(JNIEnv*, jobject) {
    if (vkManager != nullptr) {
        //vkManager->cleanup(); // done in destructor

        delete vkManager;
        vkManager = nullptr;  // Reset the pointer after deletion to avoid dangling pointer issues
    }
}



