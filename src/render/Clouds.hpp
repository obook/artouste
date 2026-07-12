/*
 * Clouds.hpp
 * Nuages en billboards (prototype), sur le modèle de render::Vegetation. Chaque
 * cumulus est un amas de bouffées (sprites blancs face caméra) réparties dans un
 * ellipsoïde à base plate ; l'ombrage (clair en haut, sombre en bas, selon le
 * soleil) donne le volume. Une couche de cumulus épars est semée au-dessus du
 * relief, à une altitude fixe.
 *
 * Contrairement aux arbres (test alpha / alpha-to-coverage, opaques), les nuages
 * demandent de la TRANSPARENCE PAR MÉLANGE avec tri de profondeur : les bouffées
 * sont donc retriées de l'arrière vers l'avant à chaque image, en fonction de la
 * position de la caméra, avant d'être téléversées et dessinées.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/Texture.hpp"
#include "util/Math.hpp"

#include <cstddef>
#include <filesystem>
#include <utility>
#include <vector>

namespace artouste::render {

class Terrain;

class Clouds {
public:
    /* Sème une couche de cumulus au-dessus du terrain (altitude déduite du relief),
       avec la texture de bouffée donnée. Sans texture, l'objet reste vide. */
    Clouds(const Terrain& terrain, const std::filesystem::path& puffPath);
    ~Clouds();

    Clouds(const Clouds&)            = delete;
    Clouds& operator=(const Clouds&) = delete;

    /* Retrie les bouffées de l'arrière vers l'avant (selon camWorld), téléverse et
       dessine (instancié). La texture est liée sur l'unité 0 ; le shader de nuages
       doit être actif et ses uniformes renseignés. Le mélange alpha et le masque de
       profondeur sont gérés par l'appelant. */
    void draw(const vec3& camWorld);

    [[nodiscard]] bool        built() const noexcept { return !m_puffs.empty(); }
    [[nodiscard]] std::size_t count() const noexcept { return m_puffs.size(); }

private:
    void release() noexcept;

    struct Puff {
        vec3  pos;    /* centre de la bouffée (monde) */
        float size;   /* rayon apparent (m) */
        float vfrac;  /* hauteur dans le nuage : 0 base (sombre), 1 sommet (clair) */
    };

    Texture                          m_puff;
    std::vector<Puff>                m_puffs;
    std::vector<std::pair<float, std::size_t>> m_order;  /* (distance^2, indice) pour le tri */
    std::vector<float>               m_instances;        /* données d'instance triées (réutilisé) */

    unsigned int m_vao         = 0;
    unsigned int m_quadVbo     = 0;
    unsigned int m_instanceVbo = 0;
    unsigned int m_ebo         = 0;
};

}  /* namespace artouste::render */
