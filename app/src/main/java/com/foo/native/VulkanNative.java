package com.foo.native;

import android.graphics.SurfaceTexture;

public class VulkanNative {
    static {
        System.loadLibrary("fluidsim");
    }

    public native boolean nativeInitialize(SurfaceTexture surface, int width, int height);
    public native void nativeDestroy();
    public native void nativeUpdate(float deltaT, float viscosity, float[] inputForce, int forceCount);
    public native void nativeAddTouchForce(int x, int y, float radius, float strength);
    public native void nativeRender();
    public native void nativePause();
    public native void nativeResume();
}
