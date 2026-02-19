#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct Vertex {
    vec3 position;
    vec3 normal;
};

struct InstanceData {
    mat4 model;
    vec4 color;
};

layout(buffer_reference, scalar) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, scalar) readonly buffer InstanceBuffer {
    InstanceData instances[];
};

layout(buffer_reference, scalar) readonly buffer CameraBuffer {
    mat4 viewProj;
    vec4 cameraPos;
};

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec3 vWorldPos;
layout(location = 2) out vec4 vColor;

layout(push_constant) uniform PushConstants {
    uint64_t vertexBuffer;
    uint64_t indexBuffer;
    uint64_t uniformBuffers[8];
    uint textures[16];
    uint samplers[16];
    float objectScale;
    uint activeTextureCount;
    uint activeSamplerCount;
    uint activeUniformCount;
    uint reserved[7];
} pc;

void main()
{
    VertexBuffer vb = VertexBuffer(pc.vertexBuffer);
    CameraBuffer camera = CameraBuffer(pc.uniformBuffers[0]);
    InstanceBuffer instances = InstanceBuffer(pc.uniformBuffers[1]);

    Vertex vertex = vb.vertices[gl_VertexIndex];
    InstanceData instance = instances.instances[gl_InstanceIndex];

    vec4 worldPos = instance.model * vec4(vertex.position * pc.objectScale, 1.0);
    gl_Position = camera.viewProj * worldPos;

    mat3 normalMatrix = mat3(instance.model);
    vNormal = normalize(normalMatrix * vertex.normal);
    vWorldPos = worldPos.xyz;
    vColor = instance.color;
}


