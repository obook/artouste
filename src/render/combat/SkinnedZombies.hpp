/*
 * SkinnedZombies.hpp
 * Rendu instancié des zombies ANIMES par squelette (skinning GPU), pendant
 * animé de render::combat::Zombies (qui, lui, dessine un modèle statique avec
 * une simple oscillation). S'appuie sur render::SkinnedModel (pack de plusieurs
 * variantes de personnages + une animation de marche commune) et sur le shader
 * zombie_skinned.
 *
 * Compromis choisi : approche par GROUPES DE PHASE. Poser un squelette par
 * instance serait trop coûteux (jusqu'à quelques centaines de zombies) ; on
 * pose donc l'animation à un petit nombre d'instants déphasés (phaseGroups),
 * et chaque zombie est rattaché à un groupe. Résultat : la marche est
 * désynchronisée de façon crédible pour un coût réduit (un jeu de matrices d'os
 * calculé par groupe et par variante réellement présente, puis un dessin
 * instancié par lot). Chaque zombie choisit sa variante et son groupe de façon
 * stable à partir d'un entier "kind" tiré à son apparition (voir
 * app::ZombieHorde), ce qui garde la horde ignorante du nombre de variantes.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/SkinnedModel.hpp"
#include "util/Math.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace artouste::render {

class Shader;

class SkinnedZombies {
public:
    /* Charge le pack skinné (modelPath) et prépare, par variante, un VAO skinné
       et un tampon d'instances dynamique d'au plus 'capacity' zombies. phaseGroups
       est le nombre d'instants déphasés de l'animation (voir en-tête). Modèle
       absent ou vide : built() reste faux, draw() ne fait rien. */
    SkinnedZombies(const std::filesystem::path& modelPath, std::size_t capacity, int phaseGroups);
    ~SkinnedZombies();

    SkinnedZombies(const SkinnedZombies&)            = delete;
    SkinnedZombies& operator=(const SkinnedZombies&) = delete;

    [[nodiscard]] bool built() const noexcept { return m_built; }

    /* Range les instances de l'image dans des lots (variante, groupe de phase)
       à partir de tableaux parallèles (même ordre/filtrage) : matrice monde,
       flash de coup touché et "kind" de chaque zombie affiché. */
    void updateInstances(const std::vector<mat4>& transforms, const std::vector<float>& hitFlashes,
                         const std::vector<int>& kinds);

    /* Dessine tous les lots. Le shader doit être en cours d'usage, ses uniformes
       de vue/projection/éclairage déjà réglés ; draw pose l'animation une fois
       par groupe de phase actif et renseigne u_bones par lot. timeSeconds est
       l'horloge d'animation (secondes, bouclée en interne). */
    void draw(Shader& shader, float timeSeconds);

private:
    void release() noexcept;

    struct Part {
        unsigned int vao         = 0;
        unsigned int vbo         = 0;  /* sommets skinnés (statique) */
        unsigned int ebo         = 0;  /* indices */
        unsigned int instanceVbo = 0;  /* matrices + flash par instance (dynamique) */
        int          indexCount  = 0;
        int          boneCount   = 0;
    };

    SkinnedModel       m_model;
    std::vector<Part>  m_parts;        /* une entrée par variante */
    std::vector<float> m_phaseOffset;  /* décalage temporel de chaque groupe (s) */
    /* Lots d'instances de l'image : index = variante * phaseGroups + groupe,
       chaque lot = 17 flottants par instance (mat4 + flash). Réutilisé d'une
       image à l'autre (clear conserve la capacité). */
    std::vector<std::vector<float>> m_buckets;
    std::size_t                     m_capacity    = 0;
    int                             m_phaseGroups = 1;
    bool                            m_built       = false;
};

}  /* namespace artouste::render */
