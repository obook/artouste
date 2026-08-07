#version 140

/*
 * monument.frag
 * Rendu des monuments 3D posés sur la carte. Deux besoins que model.frag (fait
 * pour l'appareil, toujours proche et plein) ne couvre pas :
 *
 *   - Le test alpha. Les modèles FlightGear dessinent leurs ajours dans le canal
 *     alpha de la texture : le treillis de la tour Eiffel n'est qu'un panneau
 *     plat dont 18 % des pixels sont transparents. Sans discard, la tour est un
 *     bloc opaque ; avec, on voit le ciel entre les poutrelles. On seuille plutôt
 *     que de mélanger : le mélange demanderait un tri des faces, coûteux et
 *     inutile ici, alors que le seuil écrit une vraie profondeur et laisse
 *     l'ordre de dessin indifférent.
 *
 *   - La brume. Un monument haut de 270 m se voit à des kilomètres : sans la
 *     même brume que le terrain et les bâtiments, il surgirait net sur un
 *     paysage déjà noyé.
 *
 * L'éclairage reprend le demi-Lambert de model.frag, sans la lumière d'appoint
 * caméra qui n'a de sens que dans une cabine.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec3 v_normal;
in vec2 v_uv;
in vec3 v_worldPos;

out vec4 frag_color;

uniform sampler2D u_texture;
uniform vec3  u_lightDir;    /* direction VERS la lumière, déjà normalisée */
uniform vec3  u_camPos;      /* position de la caméra (pour la distance de brume) */
uniform vec3  u_fogColor;    /* teinte de l'horizon vers laquelle on fond */
uniform float u_fogStart;    /* distance où la brume commence (m) */
uniform float u_fogEnd;      /* distance où tout est noyé dans la brume (m) */
uniform float u_alphaCutoff; /* en deçà, le pixel est jeté (ajour du treillis) */

void main() {
    vec4 albedo = texture(u_texture, v_uv);
    if (albedo.a < u_alphaCutoff) {
        discard;
    }

    /* Demi-Lambert : adoucit l'ombre sans aplatir le relief. Les poutrelles sont
       des panneaux plats dont la normale ne dit pas grand-chose du volume ;
       abs() évite qu'une face tournée à l'envers vire au noir. */
    vec3  n       = normalize(v_normal);
    float diffuse = abs(dot(n, normalize(u_lightDir))) * 0.5 + 0.5;
    vec3  color   = albedo.rgb * (0.45 + 0.55 * diffuse);

    /* Brume : proportion croissante de couleur d'horizon avec la distance,
       réglée comme celle du terrain et des bâtiments. */
    float dist = length(u_camPos - v_worldPos);
    float fog  = smoothstep(u_fogStart, u_fogEnd, dist);
    color      = mix(color, u_fogColor, fog);

    frag_color = vec4(color, 1.0);
}
