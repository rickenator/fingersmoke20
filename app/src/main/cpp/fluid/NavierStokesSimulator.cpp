#include "NavierStokesSimulator.h"
#include "Solver.h"
#include <iostream>

namespace fluidsim {

NavierStokesSimulator::NavierStokesSimulator() {
}

NavierStokesSimulator::~NavierStokesSimulator() {
    cleanup();
}

bool NavierStokesSimulator::initialize(VulkanContext* context, int width, int height) {
    mContext = context;
    mWidth = width;
    mHeight = height;
    mPaused = false;

    // Initialize the fluid solver
    mSolver = std::make_unique<fluidsim::Solver>(width, height);

    return true;
}

void NavierStokesSimulator::cleanup() {
    mSolver.reset();
    mContext = nullptr;
}

void NavierStokesSimulator::update(float deltaT, float viscosity, float* inputForce, int forceCount) {
    if (mPaused || !mSolver) {
        return;
    }

    // Update simulation
    mSolver->step(deltaT, viscosity);
}

void NavierStokesSimulator::render() {
    if (mPaused || !mContext) {
        return;
    }

    // TODO: Implement Vulkan rendering
}

void NavierStokesSimulator::pause() {
    mPaused = true;
}

void NavierStokesSimulator::resume() {
    mPaused = false;
}

void NavierStokesSimulator::addTouchForce(int x, int y, float radius, float strength) {
    if (!mSolver) {
        return;
    }

    mSolver->addTouchForce(x, y, radius, strength);
}

} // namespace fluidsim
