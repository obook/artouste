/*
 * clip_probe.cpp
 * Sonde de cadence de marche : mesure, sur le clip d'animation d'un modèle
 * skinné, la vitesse à laquelle le sol doit défiler sous le personnage pour que
 * ses pieds ne glissent pas.
 *
 * Le clip du pack de zombies est une marche SUR PLACE : sa dérive de root motion
 * oscille autour de zéro et revient à son point de départ à chaque boucle (voir
 * SkinnedModel::rootDriftXZ), il n'y a donc aucune translation à lire dedans. La
 * vitesse se déduit des pieds : un pied posé doit reculer, dans le repère du
 * personnage, exactement à la vitesse de marche. On repère donc les sommets au
 * contact du sol (la calibration pose les pieds à Y=0) et on mesure leur
 * déplacement horizontal d'un échantillon à l'autre.
 *
 * Un contexte OpenGL masqué est nécessaire : le chargement du modèle crée la
 * texture atlas embarquée. L'animation elle-même ne touche pas au GPU.
 *
 * Auteur : O. Booklage
 * Date : septembre 2026
 * Licence : GPL v2
 */

#include "render/SkinnedModel.hpp"

#include <glad/glad.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using artouste::mat4;
using artouste::vec2;
using artouste::vec3;
using artouste::vec4;
using artouste::render::SkinnedModel;

namespace {

/* Échantillons sur un cycle complet. Assez fin pour distinguer la phase d'appui
   de la phase de balancement sans rendre la mesure coûteuse. */
constexpr int   ECHANTILLONS = 240;
/* Hauteur sous laquelle un sommet est considéré au contact du sol (m). */
constexpr float SEUIL_CONTACT_M = 0.03f;

/* Positions des sommets d'une variante à l'instant t, root motion compensé --
   exactement ce que le rendu affiche (voir SkinnedZombies::posedBones). */
std::vector<vec3> sommetsPoses(const SkinnedModel& modele, std::size_t variante, float t) {
    const std::vector<mat4> globals = modele.poseAtTime(t);
    std::vector<mat4>       bones   = modele.boneMatrices(variante, globals);
    const vec2              drift   = modele.rootDriftXZ(variante, t);
    const mat4 corr = glm::translate(mat4(1.0f), vec3{-drift.x, 0.0f, -drift.y});
    for (mat4& b : bones) {
        b = corr * b;
    }

    const SkinnedModel::MeshData& md = modele.mesh(variante);
    std::vector<vec3>             out;
    out.reserve(md.vertices.size());
    for (const auto& sv : md.vertices) {
        mat4 skin(0.0f);
        for (int k = 0; k < 4; ++k) {
            skin += sv.weights[k] * bones[static_cast<std::size_t>(sv.joints[k])];
        }
        out.push_back(vec3(skin * vec4(sv.position, 1.0f)));
    }
    return out;
}

float mediane(std::vector<float> v) {
    if (v.empty()) {
        return 0.0f;
    }
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

} /* namespace */

int main(int argc, char** argv) {
    const std::string chemin =
        argc > 1 ? argv[1] : "assets/models/zombie/zombies_animated.glb";
    /* Second argument : indice de variante dont on veut le profil détaillé. */
    const int detail = argc > 2 ? std::atoi(argv[2]) : -1;

    if (glfwInit() == 0) {
        std::fprintf(stderr, "GLFW : initialisation impossible.\n");
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* fenetre = glfwCreateWindow(64, 64, "clip_probe", nullptr, nullptr);
    if (fenetre == nullptr) {
        std::fprintf(stderr, "GLFW : contexte impossible (pas d'affichage ?).\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(fenetre);
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0) {
        std::fprintf(stderr, "glad : chargement impossible.\n");
        return 1;
    }

    /* Le modèle vit dans une portée interne : sa texture atlas est détruite par
       ~SkinnedModel, qui appelle glDeleteTextures -- donc tant que le contexte
       existe encore. Sans cela le programme segmente à la sortie, en emportant
       la sortie non vidée. */
    int code = 0;
    {
    const SkinnedModel modele(chemin);
    if (!modele.built()) {
        std::fprintf(stderr, "Modèle illisible : %s\n", chemin.c_str());
        code = 1;
    } else {

    const float duree = modele.durationS();
    std::printf("Modèle   : %s\n", chemin.c_str());
    std::printf("Variantes: %zu\n", modele.meshCount());
    std::printf("Cycle    : %.4f s\n\n", static_cast<double>(duree));
    if (duree <= 1e-4f) {
        std::fprintf(stderr, "Clip sans durée : rien à mesurer.\n");
        code = 1;
    } else {

    std::printf("%-9s %14s %16s %12s\n", "variante", "vitesse (m/s)",
                "parcouru/cycle", "amplitude derive");
    std::vector<float> parVariante;

    for (std::size_t v = 0; v < modele.meshCount(); ++v) {
        const float dt = duree / static_cast<float>(ECHANTILLONS);
        /* Déplacement SIGNÉ cumulé du sol sous les pieds. C'est la bonne
           quantité : le balancement d'un pas s'annule d'un échantillon à
           l'autre, seule reste la distance dont le personnage a besoin
           d'avancer. En valeur absolue, le moindre tremblement s'ajoutait au
           lieu de se compenser et gonflait la mesure. */
        float cumulX = 0.0f;
        float cumulZ = 0.0f;

        std::vector<vec3> avant = sommetsPoses(modele, v, 0.0f);
        for (int s = 1; s <= ECHANTILLONS; ++s) {
            const float             t     = dt * static_cast<float>(s);
            const std::vector<vec3> apres = sommetsPoses(modele, v, t);
            std::vector<float>      dxs;
            std::vector<float>      dzs;
            for (std::size_t i = 0; i < avant.size() && i < apres.size(); ++i) {
                /* Au contact aux DEUX instants : un pied qui se pose ou se lève
                   pendant l'intervalle ne mesure pas le défilement du sol. */
                if (avant[i].y > SEUIL_CONTACT_M || apres[i].y > SEUIL_CONTACT_M) {
                    continue;
                }
                dxs.push_back(apres[i].x - avant[i].x);
                dzs.push_back(apres[i].z - avant[i].z);
            }
            /* Médiane : insensible au pied de balancement qui frôle le sol et
               aux quelques sommets d'orteil qui pivotent en fin d'appui. */
            if (dxs.size() >= 8) {
                cumulX += mediane(dxs);
                cumulZ += mediane(dzs);
            }
            if (detail >= 0 && static_cast<std::size_t>(detail) == v) {
                std::printf("  t=%6.3f  contacts=%4zu  cumul=(%7.3f %7.3f)\n",
                            static_cast<double>(t), dxs.size(),
                            static_cast<double>(cumulX), static_cast<double>(cumulZ));
            }
            avant = apres;
        }

        /* Le sol recule sous les pieds : la distance parcourue est l'opposée,
           mais seule la norme nous intéresse. */
        const float foulee  = std::sqrt(cumulX * cumulX + cumulZ * cumulZ);
        const float vitesse = foulee / duree;

        float driftMin = 1e9f;
        float driftMax = -1e9f;
        for (int s = 0; s <= ECHANTILLONS; ++s) {
            const vec2 d = modele.rootDriftXZ(v, dt * static_cast<float>(s));
            driftMin     = std::min(driftMin, d.y);
            driftMax     = std::max(driftMax, d.y);
        }

        parVariante.push_back(vitesse);
        std::printf("%-9zu %14.3f %14.3f %12.3f\n", v, static_cast<double>(vitesse),
                    static_cast<double>(foulee), static_cast<double>(driftMax - driftMin));
    }

    std::printf("\nMédiane toutes variantes : %.3f m/s\n",
                static_cast<double>(mediane(parVariante)));
    std::printf("\nÀ reporter dans VARIANTES_MARCHE (ZombieHordeReglages.hpp) :\n");
    std::printf("garder les variantes qui marchent vraiment, avec leur vitesse.\n");
    }
    }
    }

    std::fflush(stdout);
    glfwDestroyWindow(fenetre);
    glfwTerminate();
    return code;
}
