#version 450
layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0) buffer VelocityBuffer {
    vec2 velocities[]; // Vector field for velocities
};
layout (binding = 1) buffer PressureBuffer {
    float pressures[]; // Scalar field for pressure
};
layout (binding = 2) buffer VelocityOutput {
    vec2 outVelocities[]; // Output buffer for updated velocities
};
layout (binding = 3) buffer PressureOutput {
    float outPressures[]; // Output buffer for updated pressures
};

layout (push_constant) uniform Params {
    float deltaTime;
    float visc;
    int width;
    int height;
} params;

// Helper function to compute index from 2D coordinates
uint getIndex(uint x, uint y) {
    return y * params.width + x;
}

// Main compute function
void main() {
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;
    if (x >= params.width || y >= params.height) return;

    uint index = getIndex(x, y);

    // Handle boundaries
    if (x == 0 || y == 0 || x == params.width - 1 || y == params.height - 1) {
        outVelocities[index] = vec2(0.0); // Stick condition or customized as needed
        outPressures[index] = 0.0;
        return;
    }

    // Viscosity application (diffusion of velocity)
    vec2 laplacianV = vec2(
        velocities[getIndex(x - 1, y)] + velocities[getIndex(x + 1, y)] +
        velocities[getIndex(x, y - 1)] + velocities[getIndex(x, y + 1)] - 4.0 * velocities[index]
    );
    outVelocities[index] = velocities[index] + params.visc * params.deltaTime * laplacianV;

    // Pressure projection to maintain incompressibility
    float divergence = (
        velocities[getIndex(x + 1, y)].x - velocities[getIndex(x - 1, y)].x +
        velocities[getIndex(x, y + 1)].y - velocities[getIndex(x, y - 1)].y
    ) / 2.0;
    float pressure = (
        pressures[getIndex(x - 1, y)] + pressures[getIndex(x + 1, y)] +
        pressures[getIndex(x, y - 1)] + pressures[getIndex(x, y + 1)] - divergence
    ) / 4.0;
    outPressures[index] = pressure;
}

