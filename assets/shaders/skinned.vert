#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aNormal;
layout(location = 4) in vec4 aJoints;   // encoded as floats; cast to int in code
layout(location = 5) in vec4 aWeights;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uBones[128];

out vec3 vColor;
out vec3 vNormal;
out vec2 vUV;

void main() {
    ivec4 joints = ivec4(aJoints);
    float totalWeight = aWeights.x + aWeights.y + aWeights.z + aWeights.w;

    mat4 skin;
    if (totalWeight > 0.0) {
        skin = aWeights.x * uBones[joints.x]
             + aWeights.y * uBones[joints.y]
             + aWeights.z * uBones[joints.z]
             + aWeights.w * uBones[joints.w];
    } else {
        skin = mat4(1.0);
    }

    vec4 skinnedPos = skin * vec4(aPos, 1.0);
    vec3 skinnedNormal = mat3(skin) * aNormal;

    vColor = aColor;
    vUV = aUV;
    vNormal = normalize(mat3(uModel) * skinnedNormal);

    gl_Position = uProjection * uView * uModel * skinnedPos;
}
