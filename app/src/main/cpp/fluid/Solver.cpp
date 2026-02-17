#include "Solver.h"
#include <cstring>

namespace fluidsim {

Solver::Solver(int width, int height)
    : mWidth(width), mHeight(height) {
    mDensity = std::make_unique<Grid2D>(width, height);
    mVelocityX = std::make_unique<Grid2D>(width, height);
    mVelocityY = std::make_unique<Grid2D>(width, height);
    mPressure = std::make_unique<Grid2D>(width, height);
    mPreviousDensity = std::make_unique<Grid2D>(width, height);
    mPreviousVelocityX = std::make_unique<Grid2D>(width, height);
    mPreviousVelocityY = std::make_unique<Grid2D>(width, height);
}

Solver::~Solver() {
}

void Solver::step(float dt, float viscosity) {
    // Number of iterations for Gauss-Seidel relaxation
    const int iterations = 20;

    // Backup current state before diffusion
    backupState();

    // Diffusion step - density and velocity diffuse over time
    diffuse(dt, viscosity, iterations);

    // Advection step - density and velocity move along velocity field
    advect(dt);

    // Projection step - make velocity field incompressible (solve Poisson equation)
    project();
}

void Solver::addTouchForce(int x, int y, float radius, float strength) {
    // Add force to velocity grid at touch position
    int centerX = x * mWidth;
    int centerY = y * mHeight;

    int radiusInt = radius * std::min(mWidth, mHeight);

    for (int dy = -radiusInt; dy <= radiusInt; dy++) {
        for (int dx = -radiusInt; dx <= radiusInt; dx++) {
            int px = centerX + dx;
            int py = centerY + dy;

            if (px >= 0 && px < mWidth && py >= 0 && py < mHeight) {
                float distSq = dx * dx + dy * dy;
                if (distSq <= radiusInt * radiusInt) {
                    float falloff = 1.0f - (distSq / (radiusInt * radiusInt));
                    mVelocityX->set(px, py, mVelocityX->get(px, py) + dx * falloff * strength);
                    mVelocityY->set(px, py, mVelocityY->get(px, py) + dy * falloff * strength);
                }
            }
        }
    }
}

void Solver::backupState() {
    // Copy current state to previous state buffers
    std::memcpy(mPreviousDensity->getData(), mDensity->getData(), mWidth * mHeight * sizeof(float));
    std::memcpy(mPreviousVelocityX->getData(), mVelocityX->getData(), mWidth * mHeight * sizeof(float));
    std::memcpy(mPreviousVelocityY->getData(), mVelocityY->getData(), mWidth * mHeight * sizeof(float));
}

void Solver::diffuse(float dt, float viscosity, int iterations) {
    // Calculate diffusion coefficient: a = dt * viscosity * (width * height)^2
    // The spatial factor comes from the discretization: d^2/dx^2 ~ 1/dx^2 where dx = 1/width
    float a = dt * viscosity * (mWidth - 2) * (mHeight - 2);

    // Diffuse density using Gauss-Seidel relaxation
    solveDensity(dt, viscosity, iterations);

    // Diffuse velocity using Gauss-Seidel relaxation
    solveVelocityX(dt, viscosity, iterations);
    solveVelocityY(dt, viscosity, iterations);
}

void Solver::solveDensity(float dt, float viscosity, int iterations) {
    float* prev = mPreviousDensity->getData();
    float* curr = mDensity->getData();
    float a = dt * viscosity * (mWidth - 2) * (mHeight - 2);
    float denom = 1.0f + 4.0f * a;

    for (int iter = 0; iter < iterations; iter++) {
        for (int y = 1; y < mHeight - 1; y++) {
            for (int x = 1; x < mWidth - 1; x++) {
                int idx = x + y * mWidth;
                float sum = prev[idx] + a * (
                    curr[idx - 1] + curr[idx + 1] +
                    curr[idx - mWidth] + curr[idx + mWidth]
                );
                curr[idx] = sum / denom;
            }
        }
    }
}

void Solver::solveVelocityX(float dt, float viscosity, int iterations) {
    float* prev = mPreviousVelocityX->getData();
    float* curr = mVelocityX->getData();
    float a = dt * viscosity * (mWidth - 2) * (mHeight - 2);
    float denom = 1.0f + 4.0f * a;

    for (int iter = 0; iter < iterations; iter++) {
        for (int y = 1; y < mHeight - 1; y++) {
            for (int x = 1; x < mWidth - 1; x++) {
                int idx = x + y * mWidth;
                float sum = prev[idx] + a * (
                    curr[idx - 1] + curr[idx + 1] +
                    curr[idx - mWidth] + curr[idx + mWidth]
                );
                curr[idx] = sum / denom;
            }
        }
    }
}

void Solver::solveVelocityY(float dt, float viscosity, int iterations) {
    float* prev = mPreviousVelocityY->getData();
    float* curr = mVelocityY->getData();
    float a = dt * viscosity * (mWidth - 2) * (mHeight - 2);
    float denom = 1.0f + 4.0f * a;

    for (int iter = 0; iter < iterations; iter++) {
        for (int y = 1; y < mHeight - 1; y++) {
            for (int x = 1; x < mWidth - 1; x++) {
                int idx = x + y * mWidth;
                float sum = prev[idx] + a * (
                    curr[idx - 1] + curr[idx + 1] +
                    curr[idx - mWidth] + curr[idx + mWidth]
                );
                curr[idx] = sum / denom;
            }
        }
    }
}

void Solver::advect(float dt) {
    // Backtrace advection - move density and velocity along velocity field
    float* srcDensity = mPreviousDensity->getData();
    float* dstDensity = mDensity->getData();

    float* srcVX = mPreviousVelocityX->getData();
    float* dstVX = mVelocityX->getData();

    float* srcVY = mPreviousVelocityY->getData();
    float* dstVY = mVelocityY->getData();

    float halfWidth = (mWidth - 2) * 0.5f;
    float halfHeight = (mHeight - 2) * 0.5f;

    for (int y = 1; y < mHeight - 1; y++) {
        for (int x = 1; x < mWidth - 1; x++) {
            int idx = x + y * mWidth;

            // Backtrace: find position in previous state
            float backX = x - dt * srcVX[idx] * halfWidth;
            float backY = y - dt * srcVY[idx] * halfHeight;

            // Clamp to grid boundaries
            backX = std::max(1.0f, std::min(mWidth - 2.0f, backX));
            backY = std::max(1.0f, std::min(mHeight - 2.0f, backY));

            // Bilinear interpolation
            int x0 = (int)backX;
            int y0 = (int)backY;
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            float fx = backX - x0;
            float fy = backY - y0;

            // Interpolate density
            float d00 = srcDensity[x0 + y0 * mWidth];
            float d01 = srcDensity[x0 + y1 * mWidth];
            float d10 = srcDensity[x1 + y0 * mWidth];
            float d11 = srcDensity[x1 + y1 * mWidth];
            dstDensity[idx] = d00 * (1.0f - fx) * (1.0f - fy) +
                              d10 * fx * (1.0f - fy) +
                              d01 * (1.0f - fx) * fy +
                              d11 * fx * fy;

            // Interpolate velocity X
            float vx00 = srcVX[x0 + y0 * mWidth];
            float vx01 = srcVX[x0 + y1 * mWidth];
            float vx10 = srcVX[x1 + y0 * mWidth];
            float vx11 = srcVX[x1 + y1 * mWidth];
            dstVX[idx] = vx00 * (1.0f - fx) * (1.0f - fy) +
                         vx10 * fx * (1.0f - fy) +
                         vx01 * (1.0f - fx) * fy +
                         vx11 * fx * fy;

            // Interpolate velocity Y
            float vy00 = srcVY[x0 + y0 * mWidth];
            float vy01 = srcVY[x0 + y1 * mWidth];
            float vy10 = srcVY[x1 + y0 * mWidth];
            float vy11 = srcVY[x1 + y1 * mWidth];
            dstVY[idx] = vy00 * (1.0f - fx) * (1.0f - fy) +
                         vy10 * fx * (1.0f - fy) +
                         vy01 * (1.0f - fx) * fy +
                         vy11 * fx * fy;
        }
    }
}

void Solver::project() {
    // Solve Poisson equation for pressure to make velocity field divergence-free
    float* divergence = mPressure->getData();
    float* pressure = mPressure->getData();

    float halfWidth = (mWidth - 2) * 0.5f;
    float halfHeight = (mHeight - 2) * 0.5f;

    // Calculate divergence
    for (int y = 1; y < mHeight - 1; y++) {
        for (int x = 1; x < mWidth - 1; x++) {
            int idx = x + y * mWidth;

            float vx = mVelocityX->at(x, y);
            float vy = mVelocityY->at(x, y);

            float vx_left = mVelocityX->at(x - 1, y);
            float vx_right = mVelocityX->at(x + 1, y);
            float vy_bottom = mVelocityY->at(x, y - 1);
            float vy_top = mVelocityY->at(x, y + 1);

            divergence[idx] = -0.5f * halfWidth * (
                vx - vx_left +
                vx - vx_right +
                vy - vy_bottom +
                vy - vy_top
            );
        }
    }

    // Solve Poisson equation using Gauss-Seidel
    float* p = mPressure->getData();
    float denom = 1.0f / (4.0f);

    for (int iter = 0; iter < 20; iter++) {
        for (int y = 1; y < mHeight - 1; y++) {
            for (int x = 1; x < mWidth - 1; x++) {
                int idx = x + y * mWidth;
                p[idx] = (divergence[idx] + p[idx - 1] + p[idx + 1] + p[idx - mWidth] + p[idx + mWidth]) * denom;
            }
        }
    }

    // Subtract pressure gradient from velocity
    for (int y = 1; y < mHeight - 1; y++) {
        for (int x = 1; x < mWidth - 1; x++) {
            int idx = x + y * mWidth;

            float vx = mVelocityX->at(x, y);
            float vy = mVelocityY->at(x, y);

            mVelocityX->at(x, y) = vx - 0.5f * halfWidth * (p[idx + 1] - p[idx - 1]);
            mVelocityY->at(x, y) = vy - 0.5f * halfHeight * (p[idx + mWidth] - p[idx - mWidth]);
        }
    }

    // Zero out boundary velocities
    for (int x = 0; x < mWidth; x++) {
        mVelocityX->at(x, 0) = 0;
        mVelocityY->at(x, 0) = 0;
        mVelocityX->at(x, mHeight - 1) = 0;
        mVelocityY->at(x, mHeight - 1) = 0;
    }

    for (int y = 0; y < mHeight; y++) {
        mVelocityX->at(0, y) = 0;
        mVelocityX->at(mWidth - 1, y) = 0;
        mVelocityY->at(0, y) = 0;
        mVelocityY->at(mWidth - 1, y) = 0;
    }
}

} // namespace fluidsim