#ifndef NAVIER_STOKES_SIMULATOR_H
#define NAVIER_STOKES_SIMULATOR_H

#include "VulkanContext.h"
#include <memory>

namespace fluidsim {

class NavierStokesSimulator {
public:
    NavierStokesSimulator();
    ~NavierStokesSimulator();

    bool initialize(VulkanContext* context, int width, int height);
    void cleanup();

    void update(float deltaT, float viscosity, float* inputForce, int forceCount);
    void addTouchForce(int x, int y, float radius, float strength);
    void render();
    void pause();
    void resume();

private:
    VulkanContext* mContext = nullptr;
    int mWidth, mHeight;
    std::unique_ptr<fluidsim::Solver> mSolver;
    bool mPaused = false;
};

} // namespace fluidsim

#endif // NAVIER_STOKES_SIMULATOR_H
