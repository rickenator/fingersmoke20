#version 450 core

uniform sampler2D uDensityTexture;
uniform float uScale;

in vec2 vTexCoord;
out vec4 fragColor;

void main() {
    float density = texture(uDensityTexture, vTexCoord).r * uScale;
    fragColor = vec4(density, density, density, density);
}