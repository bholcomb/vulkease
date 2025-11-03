#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// VulkEase Cube Demo - Fragment Shader (Bindless Version)
// Uses bindless textures accessed via push constants

// Bindless texture/sampler arrays created and bound by the engine:
layout(set = 0, binding = 0) uniform texture2D ve_textures[];
layout(set = 1, binding = 0) uniform sampler   ve_samplers[];

// Input from vertex shader
layout(location = 0) in vec2 fragTexCoord;

// Output color
layout(location = 0) out vec4 outColor;

// Push constants structure matching VEGraphicsPushConstants
layout(push_constant) uniform PushConstants {
    // Buffer addresses (64-bit)
    uint64_t vertexBufferAddress;  // 8 bytes
    uint64_t indexBuffer;       // 8 bytes  
    uint64_t uniformBuffers[8]; // 64 bytes
    
    // Bindless indices (32-bit)
    uint textures[16];           // 64 bytes
    uint samplers[16];           // 64 bytes
    
    // Per-object data
    float objectScale;          // 4 bytes
    uint activeTextureCount;    // 4 bytes
    uint activeSamplerCount;    // 4 bytes
    uint activeUniformCount;    // 4 bytes
    uint reserved[7];           // 28 bytes
} pc;


void main() {
    // Get the first texture and sampler indices from push constants
    uint textureIndex = pc.textures[0];
    uint samplerIndex = pc.samplers[0];
    
    // get the fragment color from the bindless texture using nonuniform indexing
    vec4 textureColor = texture(
        sampler2D(ve_textures[nonuniformEXT(textureIndex)], ve_samplers[nonuniformEXT(samplerIndex)]), 
        fragTexCoord);

    // Output the sampled color
    outColor = textureColor;
}