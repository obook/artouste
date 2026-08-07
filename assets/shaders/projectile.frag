#version 140

/*
 * projectile.frag
 * Forme procédurale (pas de texture) : un anneau (pneu/tore) plutôt qu'un
 * disque plein -- le billboard fait toujours face à la caméra (voir
 * projectile.vert), donc un tore s'y dessine simplement comme un anneau,
 * sans avoir besoin d'un vrai maillage 3D ni d'orientation à gérer. Découpe
 * nette (alpha-test) plutôt que mélangée -- cohérent avec le reste du rendu
 * (zombie.frag, vegetation.frag), pas de gestion d'état de mélange à ajouter
 * au rendu.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec2 v_uv;

out vec4 frag_color;

void main() {
    vec2  p = v_uv - vec2(0.5);
    float d = length(p) * 2.0;  /* 0 au centre, 1 au bord du quad */
    /* Trou central du pneu : sous ce rayon, on voit "à travers". */
    const float INNER_R = 0.42;
    if (d > 1.0 || d < INNER_R) {
        discard;
    }

    /* Position radiale dans l'épaisseur du boudin (0 = bord interne,
       1 = bord externe), utilisée pour simuler le profil arrondi du pneu :
       plus clair au sommet du boudin (t = 0,5), assombri vers ses deux bords
       (occlusion), comme une coupe ronde de caoutchouc vue de face. */
    float t       = (d - INNER_R) / (1.0 - INNER_R);
    float profile = sin(t * 3.14159265);

    vec3 rubber    = vec3(0.05, 0.045, 0.04);  /* caoutchouc presque noir */
    vec3 highlight = rubber * 3.0;             /* sommet du boudin, plus clair */
    vec3 color     = mix(rubber * 0.5, highlight, profile);
    frag_color     = vec4(color, 1.0);
}
