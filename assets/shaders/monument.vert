#version 140
#extension GL_ARB_explicit_attrib_location : require

/*
 * monument.vert
 * Shader de sommets des monuments 3D : identique à model.vert (transformation
 * MVP, normale et position monde, UV), mais séparé parce que le fragment qui
 * l'accompagne ajoute le test alpha et la brume, dont l'appareil n'a que faire.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_color;
layout(location = 3) in vec2 a_uv;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out vec3 v_normal;
out vec2 v_uv;
out vec3 v_worldPos;

/* La matrice de pose porte une échelle non uniforme (voir monuments.txt : les
   modèles ne sont pas toujours aux proportions de l'édifice), que mat3(u_model)
   ne transporte donc pas exactement sur les normales. L'écart reste de quelques
   pour cent, et le fragment renormalise avant de s'en servir : la matrice
   normale complète (inverse transposée) ne se justifie pas ici. */

void main() {
    vec4 world  = u_model * vec4(a_pos, 1.0);
    v_worldPos  = world.xyz;
    v_normal    = mat3(u_model) * a_normal;
    v_uv        = a_uv;
    gl_Position = u_proj * u_view * world;
}
