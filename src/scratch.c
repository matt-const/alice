#version 330 core

in vec3 vPosOS;
out vec4 FragColor;

uniform sampler3D volumeTex;

uniform vec3 cameraPosOS;   // camera position in object space
uniform float stepSize;     // e.g. 0.01

// Cube bounds in object space
const vec3 boxMin = vec3(0.0);
const vec3 boxMax = vec3(1.0);
.
vec2 intersectBox(vec3 rayOrigin, vec3 rayDir)
{
    vec3 invDir = 1.0 / rayDir;

    vec3 t0 = (boxMin - rayOrigin) * invDir;
    vec3 t1 = (boxMax - rayOrigin) * invDir;

    vec3 tMin = min(t0, t1);
    vec3 tMax = max(t0, t1);

    float tEnter = max(max(tMin.x, tMin.y), tMin.z);
    float tExit  = min(min(tMax.x, tMax.y), tMax.z);

    return vec2(tEnter, tExit);
}

vec4 transferFunction(float v)
{
    // Simple grayscale + opacity
    return vec4(v, v, v, v);
}

void main()
{
    // Ray setup (object space)
    vec3 rayOrigin = cameraPosOS;
    vec3 rayDir = normalize(vPosOS - cameraPosOS);

    // Ray-box intersection
    vec2 tHit = intersectBox(rayOrigin, rayDir);
    float tEnter = tHit.x;
    float tExit  = tHit.y;

    if (tExit < tEnter || tExit < 0.0)
        discard;

    tEnter = max(tEnter, 0.0);

    vec4 accumColor = vec4(0.0);
    float t = tEnter;

    const int MAX_STEPS = 512;

    for (int i = 0; i < MAX_STEPS && t <= tExit; i++)
    {
        vec3 samplePos = rayOrigin + t * rayDir;

        float value = texture(volumeTex, samplePos).r;
        vec4 sampleColor = transferFunction(value);

        // Front-to-back compositing
        accumColor.rgb += (1.0 - accumColor.a) * sampleColor.a * sampleColor.rgb;
        accumColor.a   += (1.0 - accumColor.a) * sampleColor.a;

        if (accumColor.a >= 0.99)
            break;

        t += stepSize;
    }

    FragColor = accumColor;
}

// ==================================================================






#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vPosOS;   // object-space position

void main()
{
    vPosOS = aPos;  // cube assumed in [0,1]^3
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}

