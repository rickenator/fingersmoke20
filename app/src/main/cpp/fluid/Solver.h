#ifndef SOLVER_H
#define SOLVER_H

#include "Grid2D.h"
#include <memory>

namespace fluidsim {

class Solver {
public:
    Solver(int width, int height);
    ~Solver();

    void step(float dt, float viscosity);
    void addTouchForce(int x, int y, float radius, float strength);

    Grid2D& getDensity() { return *mDensity; }
    const Grid2D& getDensity() const { return *mDensity; }

    Grid2D& getVelocityX() { return *mVelocityX; }
    Grid2D& getVelocityY() { return *mVelocityY; }

private:
    int mWidth, mHeight;
    std::unique_ptr<Grid2D> mDensity;
    std::unique_ptr<Grid2D> mVelocityX;
    std::unique_ptr<Grid2D> mVelocityY;
    std::unique_ptr<Grid2D> mPressure;
    std::unique_ptr<Grid2D> mPreviousDensity;
    std::unique_ptr<Grid2D> mPreviousVelocityX;
    std::unique_ptr<Grid2D> mPreviousVelocityY;

    void diffuse(float dt, float viscosity, int iterations);
    void advect(float dt);
    void project();

    // Helper methods for Gauss-Seidel relaxation
    void solveDensity(float dt, float viscosity, int iterations);
    void solveVelocityX(float dt, float viscosity, int iterations);
    void solveVelocityY(float dt, float viscosity, int iterations);

    // Copy current state to previous state buffers
    void backupState();
};

} // namespace fluidsim

#endif // SOLVER_H
