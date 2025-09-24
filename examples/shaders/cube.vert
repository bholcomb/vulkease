#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// VulkEase Cube Demo - Vertex Shader (Buffer Device Address Version)
// Uses buffer device address for uniform buffers

// Vertex structure matching our C struct
struct Vertex {
    vec3 position;
    vec2 uv;
};

// Buffer reference for bindless vertex access
layout(buffer_reference, scalar) readonly buffer VertexBuffer {
    Vertex vertices[];
};

// Buffer reference for uniform buffer access via device address
layout(buffer_reference, scalar) buffer UniformBuffer {
    mat4 mvpMatrix;
};

// Output to fragment shader
layout(location = 0) out vec2 fragTexCoord;

// Push constants structure matching VEGraphicsPushConstants
layout(push_constant) uniform PushConstants {
    // Buffer addresses (64-bit)
    uint64_t vertexBufferAddress;  // 8 bytes
    uint64_t indexBuffer;       // 8 bytes  
    uint64_t uniformBuffers[4]; // 32 bytes
    
    // Bindless indices (32-bit)
    uint textures[8];           // 32 bytes
    uint samplers[8];           // 32 bytes
    
    // Per-object data
    float objectScale;          // 4 bytes
    uint activeTextureCount;    // 4 bytes
    uint activeSamplerCount;    // 4 bytes
    uint activeUniformCount;    // 4 bytes
} pc;


void main() {
    // Access uniform buffer via buffer device address
    UniformBuffer uniformBuf = UniformBuffer(pc.uniformBuffers[0]);

    // Get vertex buffer reference from address
    VertexBuffer vertexBuffer = VertexBuffer(pc.vertexBufferAddress);

    // Fetch vertex data
    Vertex vertex = vertexBuffer.vertices[gl_VertexIndex];
    
    // Transform vertex position using the MVP matrix from buffer device address
    gl_Position = uniformBuf.mvpMatrix * vec4(vertex.position * pc.objectScale, 1.0);
    
    // Pass texture coordinates to fragment shader
    fragTexCoord = vertex.uv;
}