#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vWorldPos;
layout(location = 2) in vec4 vColor;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 lightDirection = normalize(vec3(0.35, 0.65, 0.25));
    vec3 normal = normalize(vNormal);

    float diffuse = max(dot(normal, lightDirection), 0.0);
    float ambient = 0.2;
    vec3 shaded = vColor.rgb * (ambient + diffuse * 0.8);

    outColor = vec4(shaded, vColor.a);
}


