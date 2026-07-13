#version 410 core

/*
 * clouds.frag
 * Rendu d'une bouffée de nuage. La bouffée est blanche et transparente (mélange
 * alpha) ; le volume vient de l'ombrage : clair au sommet du nuage, plus sombre et
 * bleuté à la base (a_vfrac), le tout atténué la nuit selon la hauteur du soleil.
 * Les bouffées lointaines se fondent dans la brume et s'effacent.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec2  v_uv;
in float v_vfrac;
in vec3  v_worldPos;

out vec4 frag_color;

uniform sampler2D u_texture;
uniform vec3      u_lightDir;   /* direction VERS le soleil, normalisée */
uniform vec3      u_camPos;     /* position caméra (distance de brume) */
uniform vec3      u_fogColor;   /* teinte d'horizon */
uniform float     u_fogStart;
uniform float     u_fogEnd;

void main() {
    vec4 texel = texture(u_texture, v_uv);
    if (texel.a < 0.004) {
        discard;
    }

    /* Ombrage vertical : base sombre et bleutée, sommet blanc éclairé. */
    vec3  bottom = vec3(0.80, 0.83, 0.88);
    vec3  top    = vec3(1.0, 1.0, 1.0);
    float grad   = smoothstep(0.0, 1.0, v_vfrac);
    vec3  color  = mix(bottom, top, grad);

    /* Jour / nuit selon la hauteur du soleil. */
    float day = clamp(u_lightDir.y * 0.4 + 0.65, 0.55, 1.0);
    color *= day;

    /* Brume : les nuages lointains prennent la teinte d'horizon puis s'effacent. */
    float dist = length(u_camPos - v_worldPos);
    float fog  = smoothstep(u_fogStart, u_fogEnd, dist);
    color      = mix(color, u_fogColor, fog);

    float alpha = texel.a * (1.0 - 0.85 * fog);
    frag_color  = vec4(color, alpha);
}
