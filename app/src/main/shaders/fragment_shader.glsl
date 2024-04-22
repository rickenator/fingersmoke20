#version 450
layout(location = 0) in vec2 TexCoords; // Texture coordinates from vertex shader
layout(location = 0) out vec4 FragColor; // Output fragment color

layout(binding = 0) uniform sampler2D velocityTexture; // Texture containing velocity data
layout(binding = 1) uniform sampler2D pressureTexture; // Texture containing pressure data
layout(binding = 2) uniform sampler1D colorRampTexture; // Color ramp for mapping density to color

void main() {
    float velocity = length(texture(velocityTexture, TexCoords).rg); // Example: using magnitude of velocity
    float pressure = texture(pressureTexture, TexCoords).r; // Direct sampling of pressure
    vec4 velocityColor = texture(colorRampTexture, velocity); // Map velocity magnitude to color
    vec4 pressureColor = texture(colorRampTexture, pressure); // Map pressure to color

    // Combine or choose one of the color outputs
    FragColor = mix(velocityColor, pressureColor, 0.5); // Example: simple mix
}
