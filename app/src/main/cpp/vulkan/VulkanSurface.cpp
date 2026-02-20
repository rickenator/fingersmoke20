#include "VulkanSurface.h"
#include <iostream>

namespace fluidsim {

VulkanSurface::VulkanSurface() {}

VulkanSurface::~VulkanSurface() {
    // Surface is destroyed by caller via destroySurface()
}

bool VulkanSurface::createSurface(VkInstance instance, ANativeWindow* window) {
    VkAndroidSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.window = window;

    VkResult result = vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &mSurface);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create Android surface: " << result << std::endl;
        return false;
    }

    return true;
}

void VulkanSurface::destroySurface(VkInstance instance) {
    if (mSurface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, mSurface, nullptr);
        mSurface = VK_NULL_HANDLE;
    }
}

} // namespace fluidsim
