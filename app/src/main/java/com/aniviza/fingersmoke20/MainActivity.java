package com.aniviza.fingersmoke20;

import android.app.Activity;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.util.Log;

public class MainActivity extends Activity {

    static {
        System.loadLibrary("fingersmoke20");
    }

    private long lastFrameTime = System.nanoTime();
    private Thread renderThread;
    private volatile boolean running = false;
    private final Object touchLock = new Object();
    private volatile float lastTouchX = 0;
    private volatile float lastTouchY = 0;
    private volatile boolean isTouching = false;
    private volatile boolean isInitialized;

    private void updateTouch(float x, float y, boolean touching) {
        synchronized (touchLock) {
            this.lastTouchX = x;
            this.lastTouchY = y;
            this.isTouching = touching;
        }
    }
    private float deltaTime() {
        long currentTime = System.nanoTime();
        float deltaTime = (currentTime - lastFrameTime) / 1_000_000_000.0f;
        lastFrameTime = currentTime;
        return deltaTime;
    }

    public void log(String msg) {
        Log.i("VulkanActivity",msg);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Set the layout based on your application requirements
        setContentView(R.layout.activity_main);

        // Make the activity full screen
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        View decorView = getWindow().getDecorView();
        int uiOptions = View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        decorView.setSystemUiVisibility(uiOptions);

        SurfaceView surfaceView = findViewById(R.id.surface_view);
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                // The Surface is "ready" for rendering
                initVulkan(holder.getSurface());
                isInitialized = true;
                log("surface created, initializing VulkanManager");
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                // Handle surface size or format changes here
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                // Cleanup Vulkan resources
                stopRenderLoop();
                isInitialized = false;
                cleanup();
            }
        });
        // Set touch listener to capture touch events
        decorView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                float width = v.getWidth();
                float height = v.getHeight();
                if (width <= 0 || height <= 0) {
                    return false;
                }

                float normalizedX = clamp01(event.getX() / width);
                float normalizedY = clamp01(event.getY() / height);

                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                    case MotionEvent.ACTION_MOVE:
                        updateTouch(normalizedX, normalizedY, true);
                        return true;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        updateTouch(normalizedX, normalizedY, false);
                        return true;
                }
                return false;
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (isInitialized)
            startRenderLoop();
    }

    @Override
    protected void onPause() {
        super.onPause();
        stopRenderLoop();
    }

    @Override
    protected void onStop() {
        super.onStop();
        stopRenderLoop();
        cleanup();
        isInitialized = false;
    }

    private void startRenderLoop() {
        log("Starting render loop");
        if (running && renderThread != null) {
            return;
        }
        running = true;
        renderThread = new Thread(() -> {
            long lastTime = System.nanoTime();
            final double targetNs = 1_000_000_000.0 / 60.0;
            double accumulator = 0.0;

            while (running) {
                long now = System.nanoTime();
                accumulator += (now - lastTime) / targetNs;
                lastTime = now;

                while (accumulator >= 1.0) {
                    doDrawFrame((float)(1.0 / 60.0));
                    accumulator -= 1.0;
                }

                try {
                    Thread.sleep(2);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
        });
        renderThread.start();
    }

    private void stopRenderLoop() {
        running = false;
        if (renderThread != null) {
            try {
                renderThread.join();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
            renderThread = null;
        }
    }

    private void doDrawFrame(float delta) {
        float x, y;
        boolean touching;

        // Copy the values to local variables to minimize the synchronization time.
        synchronized (touchLock) {
            x = lastTouchX;
            y = lastTouchY;
            touching = isTouching;
        }

        log("doDrawFrame: "+delta+" x:"+x+" y:"+y);

        // Pass the copied values to the native rendering method.
        drawFrame(delta, x, y, touching);
    }

    private native void initVulkan(Surface surface);
    private native void cleanup();
    private native void drawFrame(float delta, float x, float y, boolean isTouching);

    private static float clamp01(float value) {
        return Math.max(0f, Math.min(1f, value));
    }

}
