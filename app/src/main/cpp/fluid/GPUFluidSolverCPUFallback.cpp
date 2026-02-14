#include "GPUFluidSolver.h"
#include <cstring>
#include <algorithm>
#include <random>
#include <cmath>

namespace fluidsim {

GPULikeFluidSolver::GPULikeFluidSolver(int width, int height)
    : mWidth(width), mHeight(height) {
    mDensity = std::make_unique<Grid2D>(width, height);
    mVelocityX = std::make_unique<Grid2D>(width, height);
    mVelocityY = std::make_unique<Grid2D>(width, height);
    mPreviousDensity = std::make_unique<Grid2D>(width, height);
    mPreviousVelocityX = std::make_unique<Grid2D>(width, height);
    mPreviousVelocityY = std::make_unique<Grid2D>(width, height);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (x > width/4 && x < 3*width/4 && y > height/4 && y < 3*height/4) {
                mDensity->set(x, y, 1.0f);
                mVelocityX->set(x, y, dist(gen) - 0.5f);
                mVelocityY->set(x, y, dist(gen) - 0.5f);
            }
        }
    }
}

GPULikeFluidSolver::~GPULikeFluidSolver() {
}

void GPULikeFluidSolver::backupState() {
    std::memcpy(mPreviousDensity->getData(), mDensity->getData(), mWidth * mHeight * sizeof(float));
    std::memcpy(mPreviousVelocityX->getData(), mVelocityX->getData(), mWidth * mHeight * sizeof(float));
    std::memcpy(mPreviousVelocityY->getData(), mVelocityY->getData(), mWidth * mHeight * sizeof(float));
}

void GPULikeFluidSolver::step(float dt, float viscosity) {
    backupState();
    diffuse(0, *mPreviousDensity, *mDensity, dt, viscosity);
    diffuse(1, *mPreviousVelocityX, *mVelocityX, dt, viscosity);
    diffuse(2, *mPreviousVelocityY, *mVelocityY, dt, viscosity);
    project(*mPreviousVelocityX, *mPreviousVelocityY, *mVelocityX, *mVelocityY);
    advect(0, *mDensity, *mPreviousDensity, *mPreviousVelocityX, *mPreviousVelocityY, dt);
    advect(1, *mVelocityX, *mPreviousVelocityX, *mPreviousVelocityX, *mPreviousVelocityY, dt);
    advect(2, *mVelocityY, *mPreviousVelocityY, *mPreviousVelocityX, *mPreviousVelocityY, dt);
    project(*mVelocityX, *mVelocityY, *mPreviousVelocityX, *mPreviousVelocityY);
}

void GPULikeFluidSolver::addTouchForce(int x, int y, float radius, float strength) {
    float radiusSq = radius * radius;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int cx = x + dx;
            int cy = y + dy;
            if (cx >= 0 && cx < mWidth && cy >= 0 && cy < mHeight) {
                float distSq = dx*dx + dy*dy;
                if (distSq < radiusSq) {
                    float falloff = 1.0f - sqrt(distSq) / radius;
                    mVelocityX->set(cx, cy, mVelocityX->get(cx, cy) + dx * falloff * strength);
                    mVelocityY->set(cx, cy, mVelocityY->get(cx, cy) + dy * falloff * strength);
                }
            }
        }
    }
}

void GPULikeFluidSolver::setDensity(int x, int y, float value) {
    mDensity->set(x, y, value);
}

float GPULikeFluidSolver::getDensity(int x, int y) const {
    return mDensity->get(x, y);
}

void GPULikeFluidSolver::setVelocity(int x, int y, float vx, float vy) {
    mVelocityX->set(x, y, vx);
    mVelocityY->set(x, y, vy);
}

void GPULikeFluidSolver::getVelocity(int x, int y, float& vx, float& vy) const {
    vx = mVelocityX->get(x, y);
    vy = mVelocityY->get(x, y);
}

void GPULikeFluidSolver::diffuse(int b, Grid2D& x, const Grid2D& x0, float dt, float diff) {
    int iter = 4;
    float a = dt * diff * mWidth * mHeight;
    for (int k = 0; k < iter; k++) {
        for (int j = 1; j < mHeight - 1; j++) {
            for (int i = 1; i < mWidth - 1; i++) {
                x.set(i, j, (x0.get(i, j) + a * (x.get(i+1, j) + x.get(i-1, j) + x.get(i, j+1) + x.get(i, j-1))) / (1 + 4 * a));
            }
        }
        setBoundary(b, x);
    }
}

void GPULikeFluidSolver::project(Grid2D& velocX, Grid2D& velocY, Grid2D& p, Grid2D& div) {
    for (int j = 1; j < mHeight - 1; j++) {
        for (int i = 1; i < mWidth - 1; i++) {
            div.set(i, j, -0.5f * (velocX.get(i+1, j) - velocX.get(i-1, j) + velocY.get(i, j+1) - velocY.get(i, j-1)) / (std::min(mWidth, mHeight) / 2));
            p.set(i, j, 0);
        }
    }
    setBoundary(0, div);
    setBoundary(0, p);
    int iter = 4;
    for (int k = 0; k < iter; k++) {
        for (int j = 1; j < mHeight - 1; j++) {
            for (int i = 1; i < mWidth - 1; i++) {
                p.set(i, j, (div.get(i, j) + p.get(i+1, j) + p.get(i-1, j) + p.get(i, j+1) + p.get(i, j-1)) / 4);
            }
        }
        setBoundary(0, p);
    }
    for (int j = 1; j < mHeight - 1; j++) {
        for (int i = 1; i < mWidth - 1; i++) {
            velocX.set(i, j, velocX.get(i, j) - 0.5f * (p.get(i+1, j) - p.get(i-1, j)) * mWidth / (std::min(mWidth, mHeight) / 2));
            velocY.set(i, j, velocY.get(i, j) - 0.5f * (p.get(i, j+1) - p.get(i, j-1)) * mHeight / (std::min(mWidth, mHeight) / 2));
        }
    }
    setBoundary(1, velocX);
    setBoundary(2, velocY);
}

void GPULikeFluidSolver::advect(int b, Grid2D& d, const Grid2D& d0, const Grid2D& velocX, const Grid2D& velocY, float dt) {
    float dt0 = dt * (std::min(mWidth, mHeight) / 2);
    for (int j = 1; j < mHeight - 1; j++) {
        for (int i = 1; i < mWidth - 1; i++) {
            float x = i - dt0 * velocX.get(i, j);
            float y = j - dt0 * velocY.get(i, j);
            if (x < 0.5f) x = 0.5f;
            if (x > mWidth - 1.5f) x = mWidth - 1.5f;
            if (y < 0.5f) y = 0.5f;
            if (y > mHeight - 1.5f) y = mHeight - 1.5f;
            int i0 = static_cast<int>(x);
            int i1 = i0 + 1;
            int j0 = static_cast<int>(y);
            int j1 = j0 + 1;
            float s1 = x - i0;
            float s0 = 1.0f - s1;
            float t1 = y - j0;
            float t0 = 1.0f - t1;
            d.set(i, j, s0 * (t0 * d0.get(i0, j0) + t1 * d0.get(i0, j1)) + s1 * (t0 * d0.get(i1, j0) + t1 * d0.get(i1, j1)));
        }
    }
    setBoundary(b, d);
}

void GPULikeFluidSolver::setBoundary(int b, Grid2D& x) {
    for (int i = 1; i < mWidth - 1; i++) {
        x.set(i, 0, b == 2 ? -x.get(i, 1) : x.get(i, 1));
        x.set(i, mHeight - 1, b == 2 ? -x.get(i, mHeight - 2) : x.get(i, mHeight - 2));
    }
    for (int j = 1; j < mHeight - 1; j++) {
        x.set(0, j, b == 1 ? -x.get(1, j) : x.get(1, j));
        x.set(mWidth - 1, j, b == 1 ? -x.get(mWidth - 2, j) : x.get(mWidth - 2, j));
    }
    x.set(0, 0, 0.5f * (x.get(1, 0) + x.get(0, 1)));
    x.set(0, mHeight - 1, 0.5f * (x.get(1, mHeight - 1) + x.get(0, mHeight - 2)));
    x.set(mWidth - 1, 0, 0.5f * (x.get(mWidth - 2, 0) + x.get(mWidth - 1, 1)));
    x.set(mWidth - 1, mHeight - 1, 0.5f * (x.get(mWidth - 2, mHeight - 1) + x.get(mWidth - 1, mHeight - 2)));
}
} // namespace fluidsim
