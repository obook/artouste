#version 410 core

// Dégradé de ciel selon l'élévation du rayon de vue : horizon clair, zénith
// bleu, légère brume sous l'horizon.

in vec2 v_ndc;

out vec4 frag_color;

uniform mat4 u_invViewProj;
uniform vec3 u_camPos;

void main() {
    vec4 world = u_invViewProj * vec4(v_ndc, 1.0, 1.0);
    vec3 dir   = normalize(world.xyz / world.w - u_camPos);

    vec3 horizon = vec3(0.78, 0.84, 0.92);
    vec3 zenith  = vec3(0.24, 0.46, 0.82);
    vec3 ground  = vec3(0.55, 0.62, 0.60);

    vec3 color;
    if (dir.y >= 0.0) {
        color = mix(horizon, zenith, smoothstep(0.0, 0.55, dir.y));
    } else {
        color = mix(horizon, ground, smoothstep(0.0, 0.25, -dir.y));
    }
    frag_color = vec4(color, 1.0);
}
