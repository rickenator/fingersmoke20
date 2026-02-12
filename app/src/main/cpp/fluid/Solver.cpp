#include "Solver.h"

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
    // Simplified fluid simulation - in production you'd implement proper Navier-Stokes
    // This is a placeholder for the actual physics implementation
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

} // namespace fluidsim
