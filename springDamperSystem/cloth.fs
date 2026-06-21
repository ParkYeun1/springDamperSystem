#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3  viewPos;
uniform vec3  lightDir;
uniform vec3  lightColor;
uniform vec3  objectColorFront;
uniform vec3  objectColorBack;
uniform float shininess;

// Two-sided directional Blinn-Phong lighting.
void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);

    // flip the normal toward the viewer; tint each side differently
    vec3 baseColor = objectColorFront;
    if (dot(N, V) < 0.0) {
        N = -N;
        baseColor = objectColorBack;
    }

    vec3 L = normalize(-lightDir);
    vec3 H = normalize(L + V);

    vec3  ambient  = 0.18 * lightColor;
    float diff     = max(dot(N, L), 0.0);
    vec3  diffuse  = diff * lightColor;
    float spec     = pow(max(dot(N, H), 0.0), shininess);
    vec3  specular = 0.3 * spec * lightColor;

    vec3 result = (ambient + diffuse) * baseColor + specular;
    FragColor = vec4(result, 1.0);
}
