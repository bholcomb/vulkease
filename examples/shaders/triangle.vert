#version 450 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// Vertex structure matching our C struct
struct Vertex {
    vec3 position;
    vec4 color;
};

// Buffer reference for bindless vertex access
layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

// Output to fragment shader
layout(location = 0) out vec4 fragColor;

// Push constants for transformation and vertex buffer address
layout(push_constant) uniform PushConstants {
    mat4 mvpMatrix;
    uint64_t vertexBufferAddress;
} pc;

void main() {
    // Get vertex buffer reference from address
    VertexBuffer vertexBuffer = VertexBuffer(pc.vertexBufferAddress);
    
    // Fetch vertex data
    Vertex vertex = vertexBuffer.vertices[gl_VertexIndex];
    
    // Transform position and pass color
    gl_Position = pc.mvpMatrix * vec4(vertex.position, 1.0);
    fragColor = vertex.color;
}
