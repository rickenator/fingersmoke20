#ifndef ANDROID_APP_BRIDGE_H
#define ANDROID_APP_BRIDGE_H

#include <android/native_window_jni.h>
#include <memory>

namespace fluidsim {

// Forward declarations
class VulkanContext;
class NavierStokesSimulator;

class AndroidAppBridge {
public:
    AndroidAppBridge();
    ~AndroidAppBridge();

    bool initialize(ANativeWindow* window, int width, int height);
    void destroy();
    void update(float deltaT, float viscosity, float* inputForce, int forceCount);
    void addTouchForce(int x, int y, float radius, float strength);
    void render();
    void pause();
    void resume();

private:
    std::unique_ptr<NavierStokesSimulator> mSimulator;
    std::unique_ptr<VulkanContext> mVulkanContext;
    bool mInitialized;
};

} // namespace fluidsim

#endif // ANDROID_APP_BRIDGE_H
