#version 450
// water.vert — Gerstner wave vertex displacement computed in WORLD space.
//
// Key design: Gerstner displacement is applied AFTER transforming the vertex
// to world space so wave parameters (amplitude, wavelength) are true world-space
// metres. The plane mesh spans [-0.5, 0.5] local XZ and is scaled 20x in Engine,
// giving a 20x20m surface. Wavelengths of 3-10m produce clearly visible peaks.

struct PointLight {
    vec4 position;
    vec4 color;
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 priorViewProjection;
    vec4 ambientLightColor;
    PointLight pointLights[10];
    int   numLights;
    float time;
    float deltaTime;   // seconds since previous frame (for motion vectors)
    float _pad1;
    float _pad2;
    float _pad3;
    float _pad4;
    float _pad5;
    mat4  inverseProjection;   // exact NDC-depth -> view-space unprojection
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 priorModelMatrix;
    vec4 baseColor;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormalWorld;
layout(location = 3) out vec4 fragCurrentClip;
layout(location = 4) out vec4 fragPriorClip;
layout(location = 5) out vec2 fragTexCoord;
layout(location = 6) out vec3 fragTangentWorld;
layout(location = 7) out vec3 fragBitangentWorld;
layout(location = 8) out float fragFoamCrest;

// ---- Gerstner wave (world-space XZ) ----------------------------------------
// All spatial units are world-space metres.
struct WaveParams {
    vec2  dir;         // normalised XZ propagation direction
    float amplitude;   // half crest-to-trough height (metres)
    float wavelength;  // metres per full cycle
    float speed;       // phase speed multiplier (1.0 = deep-water dispersion)
    float steepness;   // Q: 0 = sine, higher = sharper crests (keep < 1/N_waves)
};

const float kGravity = 9.81;
const float kTwoPi   = 6.28318530718;

vec3 gerstnerWave(WaveParams w, vec3 worldPos, float t,
                  inout vec3 tangent, inout vec3 binormal)
{
    float k     = kTwoPi / w.wavelength;
    float omega = sqrt(kGravity * k) * w.speed;
    float phase = k * dot(w.dir, worldPos.xz) - omega * t;
    float sinPh = sin(phase);
    float cosPh = cos(phase);
    float Q     = w.steepness;

    // Trochoidal displacement
    vec3 delta;
    delta.x = Q * w.amplitude * w.dir.x * cosPh;
    delta.y =     w.amplitude            * sinPh;
    delta.z = Q * w.amplitude * w.dir.y * cosPh;

    // Analytic surface derivatives
    float kAsin = k * w.amplitude * sinPh;
    float kAcos = k * w.amplitude * cosPh;
    tangent  -= vec3(Q * w.dir.x * w.dir.x * kAsin,
                     w.dir.x * kAcos,
                     Q * w.dir.x * w.dir.y * kAsin);
    binormal -= vec3(Q * w.dir.x * w.dir.y * kAsin,
                     w.dir.y * kAcos,
                     Q * w.dir.y * w.dir.y * kAsin);
    return delta;
}

void main()
{
    // World-space wave parameters for a 20x20m surface.
    // Amplitudes 0.1-0.4m, wavelengths 2-10m → multiple visible peaks.
    WaveParams waves[4];
    waves[0] = WaveParams(normalize(vec2( 1.0,  0.5)), 0.35, 9.0, 1.0, 0.28);
    waves[1] = WaveParams(normalize(vec2(-0.4,  1.0)), 0.22, 5.5, 1.2, 0.22);
    waves[2] = WaveParams(normalize(vec2( 0.3, -0.8)), 0.14, 3.2, 1.5, 0.18);
    waves[3] = WaveParams(normalize(vec2(-0.7,  0.3)), 0.08, 2.0, 1.8, 0.14);

    // Transform to world space — Gerstner is evaluated here so wavelengths are metres.
    vec3 worldBase = (push.modelMatrix * vec4(inPosition, 1.0)).xyz;

    vec3 pos     = worldBase;
    vec3 tangent = vec3(1.0, 0.0, 0.0);
    vec3 binormal = vec3(0.0, 0.0, 1.0);

    for (int i = 0; i < 4; i++)
    {
        pos += gerstnerWave(waves[i], worldBase, ubo.time, tangent, binormal);
    }

    // Surface normal from analytic wave derivatives — already world-space.
    vec3 waveNormal = normalize(cross(binormal, tangent));

    // Prior-frame position: re-evaluate the SAME wave field at the previous
    // frame's time using the previous model matrix, so motion vectors capture
    // wave movement (not just camera/object movement).
    float priorTime      = ubo.time - ubo.deltaTime;
    vec3  priorWorldBase = (push.priorModelMatrix * vec4(inPosition, 1.0)).xyz;
    vec3  priorPos       = priorWorldBase;
    vec3  priorTangent   = vec3(1.0, 0.0, 0.0);   // derivatives unused for position
    vec3  priorBinormal  = vec3(0.0, 0.0, 1.0);
    for (int i = 0; i < 4; i++)
    {
        priorPos += gerstnerWave(waves[i], priorWorldBase, priorTime, priorTangent, priorBinormal);
    }

    // Clip-space position from displaced world position.
    fragCurrentClip = ubo.projection * ubo.view * vec4(pos, 1.0);
    fragPriorClip   = ubo.priorViewProjection * vec4(priorPos, 1.0);
    gl_Position     = fragCurrentClip;

    // World-space tangent frame for normal mapping in the fragment shader.
    fragNormalWorld    = waveNormal;
    fragTangentWorld   = normalize(tangent);
    fragBitangentWorld = normalize(cross(waveNormal, fragTangentWorld));

    fragPosWorld  = pos;
    fragTexCoord  = inUV;
    fragColor     = push.baseColor.xyz;

    // Pass raw wave height (metres above rest plane) to the fragment shader.
    // The fragment shader applies the full falloff curve for foam shaping.
    fragFoamCrest = pos.y - worldBase.y;
}
