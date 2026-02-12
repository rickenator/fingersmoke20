#ifndef VULKAN_SURFACE_H
#define VULKAN_SURFACE_H

#include <vulkan/vulkan.h>
#include <android/native_window_jni.h>

namespace fluidsim {

class VulkanSurface {
public:
    VulkanSurface();
    ~VulkanSurface();

    bool createSurface(VkInstance instance, ANativeWindow* window);
    void destroySurface(VkInstance instance);

    VkSurfaceKHR getSurface() const { return mSurface; }

private:
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
};

} // namespace fluidsim

#endif // VULKAN_SURFACE_H
