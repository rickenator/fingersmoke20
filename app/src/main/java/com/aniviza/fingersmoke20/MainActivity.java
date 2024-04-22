package com.aniviza.fingersmoke20;

import android.app.Activity;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;

public class MainActivity extends Activity {

    static {
        System.loadLibrary("fingersmoke20");
    }

    private long lastFrameTime = System.nanoTime();
    private Thread renderThread;
    private volatile boolean running = false;
    private volatile float lastTouchX = 0;
    private volatile float lastTouchY = 0;
    private volatile boolean isTouching = false;

    private void updateTouch(float x, float y, boolean touching) {
        this.lastTouchX = x;
        this.lastTouchY = y;
        this.isTouching = touching;
    }
    private float deltaTime() {
        long currentTime = System.nanoTime();
        float deltaTime = (currentTime - lastFrameTime) / 1_000_000_000.0f;
        lastFrameTime = currentTime;
        return deltaTime;
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
                // The Surface is ready for rendering
                initVulkan(holder.getSurface());
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                // Handle surface size or format changes here
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                // Cleanup Vulkan resources
                cleanup();
            }
        });
        // Set touch listener to capture touch events
        decorView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                    case MotionEvent.ACTION_MOVE:
                        // Normalize x, y coordinates by the view's width and height
                        updateTouch(event.getX() / v.getWidth(), event.getY() / v.getHeight(), true);
                        return true;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        updateTouch(event.getX() / v.getWidth(), event.getY() / v.getHeight(), false);
                        return true;
                }
                return false;
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        startRenderLoop();
    }

    @Override
    protected void onPause() {
        stopRenderLoop();
        super.onPause();
    }

    private void startRenderLoop() {
        running = true;
        renderThread = new Thread(() -> {
            long lastTime = System.nanoTime();
            final double ns = 1000000000.0 / 60.0;  // 60 frames per second
            double delta = 0;

            while (running) {
                long now = System.nanoTime();
                delta += (now - lastTime);// / ns;
                lastTime = now;

                while (delta/ns >= 1) {
                    doDrawFrame((float)delta);  // Pass fixed delta time
                    delta -= 1;
                }

                try {
                    Thread.sleep(8);  // Sleep a little to yield time to the system
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
        });
        renderThread.start();
    }

    private void stopRenderLoop() {
        running = false;
        try {
            renderThread.join();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    private void doDrawFrame(float delta) {
        float x, y;
        boolean touching;

        // Copy the values to local variables to minimize the synchronization time.
        synchronized (this) {
            x = lastTouchX;
            y = lastTouchY;
            touching = isTouching;
        }

        // Pass the copied values to the native rendering method.
        drawFrame(delta, x, y, touching);
    }

    private native void initVulkan(Surface surface);
    private native void cleanup();
    private native void drawFrame(float delta, float x, float y, boolean isTouching);

}
