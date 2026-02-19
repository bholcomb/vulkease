#version 450 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// Particle structure matching the C struct
struct Particle {
    vec2 position;
    vec2 velocity;
    float life;
    float size;
    vec4 color;
    float padding; // Align to 16 bytes
};

// Buffer references for bindless access
layout(buffer_reference, std430) readonly buffer VertexBuffer {
    vec2 vertices[];
};

layout(buffer_reference, std430) readonly buffer ParticleBuffer {
    Particle particles[];
};

// Output to fragment shader
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

// Push constants for graphics parameters
layout(push_constant) uniform GraphicsPushConstants {
    uint64_t vertexBufferAddress;
    uint64_t particleBufferAddress;
    vec2 screenSize;
    vec2 padding;
} pc;

void main() {
    // Get buffer references
    VertexBuffer vertexBuffer = VertexBuffer(pc.vertexBufferAddress);
    ParticleBuffer particleBuffer = ParticleBuffer(pc.particleBufferAddress);
    
    // Get vertex data (quad vertex for this instance)
    vec2 quadVertex = vertexBuffer.vertices[gl_VertexIndex];
    
    // Get particle data for this instance
    Particle particle = particleBuffer.particles[gl_InstanceIndex];
    
    // Scale quad vertex by particle size
    vec2 scaledVertex = quadVertex * particle.size;
    
    // Translate to particle position
    vec2 worldPos = particle.position + scaledVertex;
    
    // Convert to clip space
    gl_Position = vec4(worldPos, 0.0, 1.0);
    
    // Pass color and texture coordinates to fragment shader
    fragColor = particle.color;
    fragTexCoord = quadVertex + 0.5; // Convert from [-0.5, 0.5] to [0, 1]
}
