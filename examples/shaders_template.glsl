// Basic triangle vertex shader
// shaders/triangle.vert
#version 450

// Push constants containing buffer addresses
layout(push_constant) uniform PushConstants {
    uint64_t vertexBuffer;  // Buffer device address
    vec3 padding;
} pc;

// Vertex structure (matches C struct)
struct Vertex {
    vec3 position;
    vec4 color;
};

layout(location = 0) out vec4 fragColor;

void main() {
    // Load vertex data using buffer device address
    Vertex vertex = Vertex(
        unpack32(uvec2(pc.vertexBuffer + gl_VertexIndex * 28)),      // position (3 floats = 12 bytes)
        unpack32(uvec2(pc.vertexBuffer + gl_VertexIndex * 28 + 12))  // color (4 floats = 16 bytes)
    );
    
    gl_Position = vec4(vertex.position, 1.0);
    fragColor = vertex.color;
}

---

// Basic triangle fragment shader
// shaders/triangle.frag
#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = fragColor;
}

---

// Textured quad vertex shader
// shaders/textured_quad.vert
#version 450

layout(push_constant) uniform PushConstants {
    uint64_t vertexBuffer;
    uint64_t indexBuffer;
    uint64_t uniformBuffer;
    uint diffuseTexture;
    uint sampler;
    vec3 padding;
} pc;

struct Vertex {
    vec3 position;
    vec2 texCoord;
};

struct Matrices {
    mat4 model;
    mat4 view;
    mat4 projection;
};

layout(location = 0) out vec2 fragTexCoord;

void main() {
    // Load vertex data
    Vertex vertex = Vertex(
        unpack32(uvec2(pc.vertexBuffer + gl_VertexIndex * 20)),      // position
        unpack32(uvec2(pc.vertexBuffer + gl_VertexIndex * 20 + 12))  // texCoord
    );
    
    // Load matrices from uniform buffer
    Matrices matrices = Matrices(
        unpack32(uvec2(pc.uniformBuffer)),          // model
        unpack32(uvec2(pc.uniformBuffer + 64)),     // view
        unpack32(uvec2(pc.uniformBuffer + 128))     // projection
    );
    
    gl_Position = matrices.projection * matrices.view * matrices.model * vec4(vertex.position, 1.0);
    fragTexCoord = vertex.texCoord;
}

---

// Textured quad fragment shader
// shaders/textured_quad.frag
#version 450

layout(push_constant) uniform PushConstants {
    uint64_t vertexBuffer;
    uint64_t indexBuffer;
    uint64_t uniformBuffer;
    uint diffuseTexture;
    uint sampler;
    vec3 padding;
} pc;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    // Sample texture using bindless texture and sampler
    outColor = texture(sampler2D(texture2D[pc.diffuseTexture], sampler[pc.sampler]), fragTexCoord);
}

---

// Particle compute shader
// shaders/particles.comp
#version 450

layout(local_size_x = 64) in;

struct Particle {
    vec2 position;
    vec2 velocity;
    float life;
    float size;
    vec4 color;
    float padding;
};

layout(push_constant) uniform PushConstants {
    uint64_t particleBuffer;
    float deltaTime;
    float time;
    vec2 attractorPos;
    float attractorStrength;
    uint particleCount;
} pc;

void main() {
    uint index = gl_GlobalInvocationID.x;
    if (index >= pc.particleCount) return;
    
    // Load particle data
    uint64_t particleAddr = pc.particleBuffer + index * 36; // sizeof(Particle)
    Particle particle = Particle(
        unpack32(uvec2(particleAddr)),          // position
        unpack32(uvec2(particleAddr + 8)),      // velocity  
        unpack32(uvec2(particleAddr + 16)),     // life
        unpack32(uvec2(particleAddr + 20)),     // size
        unpack32(uvec2(particleAddr + 24)),     // color
        0.0                                     // padding
    );
    
    // Update particle physics
    vec2 toAttractor = pc.attractorPos - particle.position;
    float distance = length(toAttractor);
    vec2 force = normalize(toAttractor) * pc.attractorStrength / (distance * distance + 0.01);
    
    particle.velocity += force * pc.deltaTime;
    particle.position += particle.velocity * pc.deltaTime;
    particle.life -= pc.deltaTime;
    
    // Respawn if dead
    if (particle.life <= 0.0) {
        float angle = float(index) * 0.1 + pc.time;
        particle.position = vec2(cos(angle), sin(angle)) * 0.5;
        particle.velocity = vec2(0.0);
        particle.life = 5.0;
    }
    
    // Store particle data back
    pack32(uvec2(particleAddr), particle.position);
    pack32(uvec2(particleAddr + 8), particle.velocity);
    pack32(uvec2(particleAddr + 16), particle.life);
}

---

// Particle vertex shader
// shaders/particles.vert
#version 450

layout(push_constant) uniform PushConstants {
    uint64_t vertexBuffer;
    uint64_t particleBuffer;
    vec2 screenSize;
    vec2 padding;
} pc;

struct Particle {
    vec2 position;
    vec2 velocity;
    float life;
    float size;
    vec4 color;
    float padding;
};

layout(location = 0) out vec4 fragColor;

void main() {
    // Load quad vertex (4 vertices: -0.5,-0.5  0.5,-0.5  0.5,0.5  -0.5,0.5)
    vec2 quadVertex = vec2(
        unpack32(uvec2(pc.vertexBuffer + gl_VertexIndex * 8))
    );
    
    // Load particle data for this instance
    uint particleIndex = gl_InstanceIndex;
    uint64_t particleAddr = pc.particleBuffer + particleIndex * 36;
    Particle particle = Particle(
        unpack32(uvec2(particleAddr)),          // position
        unpack32(uvec2(particleAddr + 8)),      // velocity
        unpack32(uvec2(particleAddr + 16)),     // life
        unpack32(uvec2(particleAddr + 20)),     // size
        unpack32(uvec2(particleAddr + 24)),     // color
        0.0
    );
    
    // Transform quad vertex to particle position and size
    vec2 worldPos = particle.position + quadVertex * particle.size;
    gl_Position = vec4(worldPos, 0.0, 1.0);
    
    // Alpha based on life
    fragColor = particle.color;
    fragColor.a *= clamp(particle.life / 5.0, 0.0, 1.0);
}

---

// Particle fragment shader
// shaders/particles.frag
#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    // Create circular particle
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float distance = length(coord);
    if (distance > 1.0) discard;
    
    float alpha = 1.0 - distance * distance;
    outColor = vec4(fragColor.rgb, fragColor.a * alpha);
}

---

// README for shader compilation
// shaders/README.md

# VulkEase Example Shaders

These GLSL shaders demonstrate the bindless and buffer device address features of VulkEase 2.0.

## Key Features Demonstrated:

1. **Buffer Device Address**: Vertices and uniforms are accessed via 64-bit addresses
2. **Bindless Textures**: Textures accessed by index rather than descriptors  
3. **Push Constants**: Minimal state passed to shaders
4. **Compute Shaders**: GPU-side particle simulation

## Compilation:

The CMakeLists.txt will automatically compile these with glslc or glslangValidator:

```bash
# Manual compilation example:
glslc triangle.vert -o triangle.vert.spv
glslc triangle.frag -o triangle.frag.spv
```

## Shader Requirements:

- Vulkan 1.3+ 
- VK_EXT_buffer_device_address
- VK_EXT_descriptor_indexing
- VK_EXT_shader_object

## Notes:

- These shaders use pseudo-code for buffer device address access
- In real implementation, you'd use proper GLSL buffer device address extensions
- The unpack32/pack32 functions are placeholders for actual BDA operations