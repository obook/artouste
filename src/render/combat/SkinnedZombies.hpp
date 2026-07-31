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
#include <utility>
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

    /* Position des deux yeux d'un zombie de ce "kind", dans le repère du modèle
       (à multiplier par sa matrice d'instance pour obtenir le repère monde).
       Suit l'os de tête de la variante, donc la pose réellement dessinée : les
       lueurs restent sur le visage au lieu de flotter devant (voir
       SkinnedModel::eyePoints).

       Lit la pose du dernier draw() : à appeler APRÈS lui dans l'image, sans
       quoi les yeux retardent d'une image. Avant le premier draw(), renvoie la
       pose de repos, calibrée au chargement. Faux si la variante n'a pas
       d'ancrage d'yeux. */
    [[nodiscard]] bool eyeAnchors(int kind, vec3& left, vec3& right) const;

private:
    void release() noexcept;

    /* Lot (variante, groupe de phase) d'un "kind" : même répartition pour les
       instances dessinées et pour les ancrages d'yeux, sans quoi les lueurs
       suivraient une autre pose que le corps. */
    [[nodiscard]] std::size_t bucketIndex(int kind) const noexcept;

    /* Matrices d'os d'une variante à l'instant tg, root motion compensé : la
       pose effectivement dessinée, dont on tire aussi les ancrages d'yeux. */
    [[nodiscard]] std::vector<mat4> posedBones(std::size_t variant,
                                               const std::vector<mat4>& globals, float tg) const;

    /* Range les deux yeux de cette variante (repère du modèle) pour ce lot. */
    void storeEyePoints(std::size_t bucket, std::size_t variant, const std::vector<mat4>& bones);

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
    /* Yeux gauche/droit par lot, dans le repère du modèle : rafraîchis par
       draw() pour chaque lot posé, à partir des mêmes matrices d'os (dérive de
       root motion comprise). Même indexation que m_buckets. */
    std::vector<std::pair<vec3, vec3>> m_eyePoints;
    std::size_t                     m_capacity    = 0;
    int                             m_phaseGroups = 1;
    bool                            m_built       = false;
};

}  /* namespace artouste::render */
