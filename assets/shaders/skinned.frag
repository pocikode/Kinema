#version 330 core
in vec3 vColor;
in vec3 vNormal;
in vec2 vUV;

uniform sampler2D baseColorTexture;
uniform vec4 uBaseColorFactor;

out vec4 FragColor;

void main() {
    vec3 n = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.3, 0.8, 0.5));
    float diffuse = max(dot(n, lightDir), 0.0);
    float ambient = 0.35;
    vec4 tex = texture(baseColorTexture, vUV) * uBaseColorFactor;
    vec3 shaded = tex.rgb * (ambient + diffuse * 0.75);
    FragColor = vec4(shaded, tex.a);
}
