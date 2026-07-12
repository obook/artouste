#version 410 core

/*
 * vegetation.frag
 * Rendu d'un arbre en billboard croisé. La transparence du sprite est traitée par
 * ALPHA-TO-COVERAGE (activé côté application, sur le multi-échantillonnage) plutôt
 * que par un simple seuil : le bord du feuillage est alors tramé sur les
 * sous-échantillons, ce qui donne des contours doux et découpés (feuilles /
 * aiguilles) au lieu d'un bord net et crénelé. On rejette tout de même les pixels
 * quasi transparents pour ne pas écrire de profondeur sur du vide.
 *
 * Un éclairage simple (hauteur du soleil), un léger assombrissement du pied de
 * l'arbre (occlusion), puis la même brume que le terrain et les bâtiments.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec2  v_uv;
in vec3  v_worldPos;
in float v_vfrac;

out vec4 frag_color;

uniform sampler2D u_texture;
uniform vec3      u_lightDir;   /* direction VERS le soleil, normalisée */
uniform vec3      u_camPos;     /* position caméra (distance de brume) */
uniform vec3      u_fogColor;   /* teinte d'horizon */
uniform float     u_fogStart;
uniform float     u_fogEnd;

void main() {
    vec4 texel = texture(u_texture, v_uv);
    if (texel.a < 0.1) {
        discard;  /* pixel quasi transparent du sprite : pas de feuillage ici */
    }

    /* Luminosité selon la hauteur du soleil (plein jour clair, nuit sombre). */
    float day = clamp(u_lightDir.y * 0.5 + 0.5, 0.25, 1.0);
    /* Très léger assombrissement du pied (occlusion ambiante) : le sprite photo est
       déjà ombré, on se contente d'accentuer un peu. */
    float ao    = mix(0.85, 1.0, clamp(v_vfrac * 1.6, 0.0, 1.0));
    vec3  color = texel.rgb * day * ao;

    /* Brume : proportion croissante de couleur d'horizon avec la distance. */
    float dist = length(u_camPos - v_worldPos);
    float fog  = smoothstep(u_fogStart, u_fogEnd, dist);
    color      = mix(color, u_fogColor, fog);

    /* On sort le vrai alpha : c'est lui qui pilote la couverture (alpha-to-coverage). */
    frag_color = vec4(color, texel.a);
}
