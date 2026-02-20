#include "android_app_bridge.h"
#include <android/log.h>
#include <jni.h>
#include <android/native_window_jni.h>

#include "VulkanContext.h"
#include "NavierStokesSimulator.h"

#define LOG_TAG "FluidSim"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global instance pointer
static fluidsim::AndroidAppBridge* gAppBridge = nullptr;

namespace fluidsim {

// Constructor
AndroidAppBridge::AndroidAppBridge()
    : mSimulator(nullptr), mInitialized(false) {
}

// Destructor
AndroidAppBridge::~AndroidAppBridge() {
    destroy();
}

// Initialize the Vulkan engine and simulation
bool AndroidAppBridge::initialize(ANativeWindow* window, int width, int height) {
    if (mInitialized) {
        return true;
    }

    LOGI("Initializing Vulkan engine...");

    try {
        // Create Vulkan context
        mVulkanContext = std::make_unique<fluidsim::VulkanContext>();
        mVulkanContext->initialize(window, width, height);

        // Create the simulator
        mSimulator = std::make_unique<fluidsim::NavierStokesSimulator>();
        mSimulator->initialize(mVulkanContext.get(), width, height);

        mInitialized = true;
        LOGI("Initialization complete. Grid size: %dx%d", width, height);

    } catch (const std::exception& e) {
        LOGE("Initialization failed: %s", e.what());
        return false;
    }

    return true;
}

// Cleanup resources
void AndroidAppBridge::destroy() {
    if (mSimulator) {
        mSimulator->cleanup();
        mSimulator.reset();
    }

    mVulkanContext.reset();
    mInitialized = false;
}

// Update simulation state
void AndroidAppBridge::update(float deltaT, float viscosity, float* inputForce, int forceCount) {
    if (!mInitialized || !mSimulator) {
        return;
    }

    mSimulator->update(deltaT, viscosity, inputForce, forceCount);
}

// Add touch force to simulation
void AndroidAppBridge::addTouchForce(int x, int y, float radius, float strength) {
    if (!mInitialized || !mSimulator) {
        return;
    }

    mSimulator->addTouchForce(x, y, radius, strength);
}

// Render frame
void AndroidAppBridge::render() {
    if (!mInitialized || !mSimulator) {
        return;
    }

    mSimulator->render();
}

// Pause simulation
void AndroidAppBridge::pause() {
    if (mSimulator) {
        mSimulator->pause();
    }
}

// Resume simulation
void AndroidAppBridge::resume() {
    if (mSimulator) {
        mSimulator->resume();
    }
}

} // namespace fluidsim

// JNI wrapper functions
extern "C" {
    JNIEXPORT jboolean JNICALL Java_com_foo_native_VulkanNative_nativeInitialize(
        JNIEnv* env, jclass clazz, jobject surface, jint width, jint height) {
        if (gAppBridge == nullptr) {
            gAppBridge = new fluidsim::AndroidAppBridge();
        }
        ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
        bool result = gAppBridge->initialize(window, width, height);
        ANativeWindow_release(window);
        return result ? JNI_TRUE : JNI_FALSE;
    }

    JNIEXPORT void JNICALL Java_com_foo_native_VulkanNative_nativeDestroy(
        JNIEnv* env, jclass clazz) {
        if (gAppBridge != nullptr) {
            gAppBridge->destroy();
            delete gAppBridge;
            gAppBridge = nullptr;
        }
    }

    JNIEXPORT void JNICALL Java_com_foo_native_VulkanNative_nativeUpdate(
        JNIEnv* env, jclass clazz, jfloat deltaT, jfloat viscosity, jfloatArray force, jint forceCount) {
        if (gAppBridge != nullptr) {
            jfloat* forceData = env->GetFloatArrayElements(force, nullptr);
            gAppBridge->update(deltaT, viscosity, forceData, forceCount);
            env->ReleaseFloatArrayElements(force, forceData, JNI_ABORT);
        }
    }

    JNIEXPORT void JNICALL Java_com_foo_native_VulkanNative_nativeRender(
        JNIEnv* env, jclass clazz) {
        if (gAppBridge != nullptr) {
            gAppBridge->render();
        }
    }

    JNIEXPORT void JNICALL Java_com_foo_native_VulkanNative_nativePause(
        JNIEnv* env, jclass clazz) {
        if (gAppBridge != nullptr) {
            gAppBridge->pause();
        }
    }

    JNIEXPORT void JNICALL Java_com_foo_native_VulkanNative_nativeResume(
        JNIEnv* env, jclass clazz) {
        if (gAppBridge != nullptr) {
            gAppBridge->resume();
        }
    }

    JNIEXPORT void JNICALL Java_com_foo_native_VulkanNative_nativeAddTouchForce(
        JNIEnv* env, jclass clazz, jint x, jint y, jfloat radius, jfloat strength) {
        if (gAppBridge != nullptr) {
            gAppBridge->addTouchForce(x, y, radius, strength);
        }
    }
}
