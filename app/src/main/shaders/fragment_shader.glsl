#version 450
layout(location = 0) in vec2 TexCoords;  // Texture coordinates from vertex shader
layout(location = 0) out vec4 FragColor; // Output fragment color

layout(binding = 0) uniform sampler2D smokeDensityTexture; // Smoke density texture
layout(binding = 1) uniform sampler1D colorRampTexture; // Color ramp for mapping density to color

void main() {
    float density = texture(smokeDensityTexture, TexCoords).r; // Get density from texture
    vec4 smokeColor = texture(colorRampTexture, density); // Map density to color using color ramp

    // Calculate alpha based on density, more density should have less transparency
    float alpha = clamp(density * 5.0, 0.0, 1.0); // Scale and clamp the density to use it as alpha

    // Set fragment color to smoke color with dynamic alpha
    FragColor = vec4(smokeColor.rgb, alpha);
}