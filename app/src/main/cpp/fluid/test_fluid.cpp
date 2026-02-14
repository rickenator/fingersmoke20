#include "GPUFluidSolver.h"
#include <iostream>
#include <chrono>
#include <cstdlib>
#include <ctime>

int main() {
    std::cout << "=== GPU Fluid Solver Test ===" << std::endl;

    std::srand(std::time(nullptr));

    // Create a fluid solver with a reasonable grid size
    const int width = 100;
    const int height = 100;
    const int iterations = 100;

    fluidsim::GPUFluidSolver solver(width, height);

    std::cout << "Fluid solver created: " << width << "x" << height << std::endl;

    // Test adding some initial density
    std::cout << "Adding initial density..." << std::endl;
    solver.setDensity(50, 50, 1.0f);
    solver.setDensity(51, 50, 1.0f);
    solver.setDensity(50, 51, 1.0f);
    solver.setDensity(51, 51, 1.0f);

    // Add some initial velocity
    solver.setVelocity(50, 50, 0.5f, 0.0f);
    solver.setVelocity(51, 50, 0.5f, 0.0f);

    // Run simulation
    std::cout << "Running simulation..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        solver.step(0.1f, 0.0001f);

        // Add a new force every 10 steps
        if (i % 10 == 0) {
            int x = width/4 + std::rand() % (width/2);
            int y = height/4 + std::rand() % (height/2);
            solver.addTouchForce(x, y, 3.0f, 0.1f);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Simulation completed in " << duration << "ms" << std::endl;

    // Check some sample values
    std::cout << "\nSample density values:" << std::endl;
    std::cout << "  Density at (50, 50): " << solver.getDensity(50, 50) << std::endl;
    std::cout << "  Density at (51, 50): " << solver.getDensity(51, 50) << std::endl;
    std::cout << "  Density at (50, 51): " << solver.getDensity(50, 51) << std::endl;
    std::cout << "  Density at (51, 51): " << solver.getDensity(51, 51) << std::endl;

    std::cout << "\nSample velocity values:" << std::endl;
    float vx, vy;
    solver.getVelocity(50, 50, vx, vy);
    std::cout << "  Velocity at (50, 50): (" << vx << ", " << vy << ")" << std::endl;
    solver.getVelocity(51, 50, vx, vy);
    std::cout << "  Velocity at (51, 50): (" << vx << ", " << vy << ")" << std::endl;
    solver.getVelocity(50, 51, vx, vy);
    std::cout << "  Velocity at (50, 51): (" << vx << ", " << vy << ")" << std::endl;
    solver.getVelocity(51, 51, vx, vy);
    std::cout << "  Velocity at (51, 51): (" << vx << ", " << vy << ")" << std::endl;

    std::cout << "\n=== Test completed successfully ===" << std::endl;

    return 0;
}
