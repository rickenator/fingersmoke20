package com.foo;

import android.graphics.SurfaceTexture;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import com.foo.native.VulkanNative;

import java.nio.FloatBuffer;

public class MainActivity extends AppCompatActivity implements View.OnTouchListener {
    private static final String TAG = "MainActivity";

    private GLSurfaceView glSurfaceView;
    private VulkanNative nativeEngine;
    private View controlPanel;
    private SeekBar viscositySeekbar;
    private SeekBar gridSpeedSeekbar;
    private TextView viscosityValue;
    private TextView gridSizeValue;

    private float currentViscosity = 0.5f;
    private int currentGridSize = 128;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Initialize native engine
        nativeEngine = new VulkanNative();

        // Setup GLSurfaceView
        glSurfaceView = findViewById(R.id.gl_surface_view);
        glSurfaceView.setEGLContextClientVersion(3);
        glSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);

        glSurfaceView.setOnTouchListener(this);
        glSurfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                Log.d(TAG, "Surface created");
                int width = holder.getSurface().getWidth();
                int height = holder.getSurface().getHeight();
                if (width == 0 || height == 0) {
                    width = 1920;
                    height = 1080;
                }
                nativeEngine.nativeInitialize(glSurfaceView.getSurfaceTexture(), width, height);
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                Log.d(TAG, "Surface changed: " + width + "x" + height);
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                Log.d(TAG, "Surface destroyed");
                nativeEngine.nativeDestroy();
            }
        });

        // Setup UI controls
        setupControls();
    }

    private void setupControls() {
        controlPanel = findViewById(R.id.control_panel);
        viscositySeekbar = findViewById(R.id.viscosity_seekbar);
        gridSpeedSeekbar = findViewById(R.id.grid_speed_seekbar);
        viscosityValue = findViewById(R.id.viscosity_value);
        gridSizeValue = findViewById(R.id.grid_size_value);

        viscositySeekbar.setMax(100);
        viscositySeekbar.setProgress(50);
        viscositySeekbar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                currentViscosity = progress / 100.0f;
                viscosityValue.setText(String.format("%.2f", currentViscosity));
                updateNativeParams();
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        gridSpeedSeekbar.setMax(100);
        gridSpeedSeekbar.setProgress(50);
        gridSpeedSeekbar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                currentGridSize = 64 + (progress * 64 / 100);
                gridSizeValue.setText(String.valueOf(currentGridSize));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
    }

    private void updateNativeParams() {
        nativeEngine.nativeUpdate(0.016f, currentViscosity, null, 0);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.main_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.toggle_controls) {
            if (controlPanel.getVisibility() == View.VISIBLE) {
                controlPanel.setVisibility(View.GONE);
            } else {
                controlPanel.setVisibility(View.VISIBLE);
            }
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (glSurfaceView != null) {
            glSurfaceView.onResume();
        }
        nativeEngine.nativeResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (glSurfaceView != null) {
            glSurfaceView.onPause();
        }
        nativeEngine.nativePause();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (glSurfaceView != null) {
            glSurfaceView.onPause();
        }
        nativeEngine.nativeDestroy();
    }

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_MOVE) {
            float x = event.getX();
            float y = event.getY();

            // Convert touch position to normalized coordinates (0-1)
            float normalizedX = x / glSurfaceView.getWidth();
            float normalizedY = 1.0f - (y / glSurfaceView.getHeight()); // Flip Y axis

            // Add touch force with default radius and strength
            nativeEngine.nativeAddTouchForce(
                (int) (normalizedX * currentGridSize),
                (int) (normalizedY * currentGridSize),
                0.1f, // radius (10% of grid)
                1.0f  // strength
            );

            return true;
        }
        return false;
    }
}
