#version 140
#extension GL_ARB_explicit_attrib_location : require

/*
 * zombie_skinned.vert
 * Modèle 3D du zombie ANIME par squelette (skinning GPU), dessiné en instancié
 * (mode zombie). Chaque sommet porte jusqu'à quatre os (a_joints) et leurs
 * poids (a_weights) ; les matrices d'os u_bones sont calculées côté CPU par
 * render::SkinnedModel pour une variante et un instant donnés, et incluent
 * déjà la correction de recentrage/échelle (localFix). La matrice d'instance
 * (locations 5-8) place ensuite le personnage dans le monde ; u_model recale
 * du repère monde vers le repère caméra, comme les autres rendus instanciés.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

layout(location = 0) in vec3  a_pos;
layout(location = 1) in vec3  a_normal;
layout(location = 2) in vec2  a_uv;
layout(location = 3) in ivec4 a_joints;   /* indices d'os (dans u_bones) */
layout(location = 4) in vec4  a_weights;  /* poids associés (somme ~1) */
/* Matrice modèle par instance (quatre colonnes vec4 consécutives). */
layout(location = 5) in vec4  a_modelCol0;
layout(location = 6) in vec4  a_modelCol1;
layout(location = 7) in vec4  a_modelCol2;
layout(location = 8) in vec4  a_modelCol3;
/* Flash de coup touché (0 = normal, 1 = vient d'être touché). */
layout(location = 9) in float a_hitFlash;
/* Graine de couleur par instance (0..1) : décale la teinte de la tenue pour
   varier les zombies (voir zombie_skinned.frag). */
layout(location = 10) in float a_colorSeed;

const int MAX_BONES = 64;
uniform mat4 u_bones[MAX_BONES];
uniform mat4 u_model;  /* recalage repère-caméra (translation -origine de rendu) */
uniform mat4 u_view;
uniform mat4 u_proj;

out vec3  v_normal;
out vec2  v_uv;
out vec3  v_worldPos;
out float v_hitFlash;
out float v_colorSeed;

void main() {
    /* Matrice de skinning : combinaison pondérée des os influents. */
    mat4 skin = a_weights.x * u_bones[a_joints.x]
              + a_weights.y * u_bones[a_joints.y]
              + a_weights.z * u_bones[a_joints.z]
              + a_weights.w * u_bones[a_joints.w];

    mat4 instanceModel = mat4(a_modelCol0, a_modelCol1, a_modelCol2, a_modelCol3);
    mat4 model         = u_model * instanceModel;

    vec4 skinned = skin * vec4(a_pos, 1.0);
    vec4 world   = model * skinned;

    v_worldPos = world.xyz;
    v_normal   = mat3(model) * mat3(skin) * a_normal;
    v_uv        = a_uv;
    v_hitFlash  = a_hitFlash;
    v_colorSeed = a_colorSeed;
    gl_Position = u_proj * u_view * world;
}
