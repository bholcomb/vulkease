#version 450 core

// Input from vertex shader
layout(location = 0) in vec4 fragColor;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    outColor = fragColor;
}
