#version 450

layout(binding = 0) uniform sampler2D indexTexture;
layout(binding = 1) uniform sampler2D paletteTexture;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    float index = texture(indexTexture, fragTexCoord).r * 255.0;
    float u = (index + 0.5) / 256.0;
    outColor = texture(paletteTexture, vec2(u, 0.5));
}
