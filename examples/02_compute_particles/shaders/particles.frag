#version 450 core

// Input from vertex shader
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // Create a circular particle by using distance from center
    vec2 center = vec2(0.5, 0.5);
    float distance = length(fragTexCoord - center);
    
    // Soft circular falloff
    float alpha = 1.0 - smoothstep(0.0, 0.5, distance);
    
    // Apply the alpha falloff and particle color
    outColor = vec4(fragColor.rgb, fragColor.a * alpha);
    
    // Discard fragments that are too transparent
    if (outColor.a < 0.01) {
        discard;
    }
}
