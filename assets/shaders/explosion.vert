#version 140
#extension GL_ARB_explicit_attrib_location : require

/*
 * explosion.vert
 * Explosion 3D animée du mode zombie (impact des roquettes) : géométrie statique
 * dessinée une fois par image d'animation, la transformation (dont l'échelle du
 * flipbook) étant portée par u_model, calculé côté CPU par render::ExplosionFx.
 * Mêmes attributs de sommet que render::Mesh (position, normale, couleur, uv) ;
 * seules la position et l'uv servent ici (rendu émissif, sans éclairage).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_color;
layout(location = 3) in vec2 a_uv;

uniform mat4 u_model;  /* recalage caméra * placement * rayon * localFix * noeud */
uniform mat4 u_view;
uniform mat4 u_proj;

out vec2 v_uv;

void main() {
    v_uv        = a_uv;
    gl_Position = u_proj * u_view * u_model * vec4(a_pos, 1.0);
}
