#include "VulkanInstance.h"
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace fluidsim {

VulkanInstance::VulkanInstance() {}

VulkanInstance::~VulkanInstance() {
    destroyInstance();
}

bool VulkanInstance::createInstance() {
    // Check for required extensions
    if (!validateExtensions()) {
        std::cerr << "Required Vulkan extensions not available" << std::endl;
        return false;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "FluidSim";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredInstanceExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Create the instance
    VkResult result = vkCreateInstance(&createInfo, nullptr, &mInstance);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance: " << result << std::endl;
        return false;
    }

    // Enumerate physical devices
    auto devices = enumeratePhysicalDevices();
    if (devices.empty()) {
        std::cerr << "No Vulkan physical devices found" << std::endl;
        return false;
    }

    // Find a suitable device
    for (auto device : devices) {
        if (isDeviceSuitable(device)) {
            mPhysicalDevice = device;
            break;
        }
    }

    if (mPhysicalDevice == VK_NULL_HANDLE) {
        std::cerr << "No suitable Vulkan physical device found" << std::endl;
        return false;
    }

    return true;
}

void VulkanInstance::destroyInstance() {
    if (mInstance != VK_NULL_HANDLE) {
        vkDestroyInstance(mInstance, nullptr);
        mInstance = VK_NULL_HANDLE;
        mPhysicalDevice = VK_NULL_HANDLE;
    }
}

std::vector<const char*> VulkanInstance::getRequiredInstanceExtensions() {
    std::vector<const char*> extensions;

    // Required for Android surface creation
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);

    // Debug validation layers (optional)
#ifdef DEBUG
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    return extensions;
}

std::vector<VkPhysicalDevice> VulkanInstance::enumeratePhysicalDevices() {
    std::vector<VkPhysicalDevice> devices;

    uint32_t deviceCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
    if (result != VK_SUCCESS || deviceCount == 0) {
        return devices;
    }

    devices.resize(deviceCount);
    result = vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());
    if (result != VK_SUCCESS) {
        devices.clear();
        return devices;
    }

    return devices;
}

bool VulkanInstance::isDeviceSuitable(VkPhysicalDevice device) {
    // Check for required extensions
    std::vector<const char*> requiredExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredSet(requiredExtensions.begin(), requiredExtensions.end());
    for (const auto& ext : availableExtensions) {
        requiredSet.erase(ext.extensionName);
    }

    if (!requiredSet.empty()) {
        return false;
    }

    // Find queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {
        return false;
    }

    return true;
}

uint32_t VulkanInstance::findGraphicsQueueFamily(VkPhysicalDevice device) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {
        return UINT32_MAX;
    }

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            return i;
        }
    }

    return UINT32_MAX;
}

uint32_t VulkanInstance::findPresentQueueFamily(VkPhysicalDevice device, VkSurfaceKHR surface) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {
        return UINT32_MAX;
    }

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport == VK_TRUE) {
            return i;
        }
    }

    return UINT32_MAX;
}

bool VulkanInstance::validateExtensions() {
    uint32_t extensionCount = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (result != VK_SUCCESS) {
        return false;
    }

    std::vector<VkExtensionProperties> extensions(extensionCount);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    if (result != VK_SUCCESS) {
        return false;
    }

    auto requiredExtensions = getRequiredInstanceExtensions();
    std::set<std::string> requiredSet(requiredExtensions.begin(), requiredExtensions.end());

    for (const auto& ext : extensions) {
        requiredSet.erase(ext.extensionName);
    }

    return requiredSet.empty();
}

} // namespace fluidsim
