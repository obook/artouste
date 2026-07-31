/*
 * ApplicationMonuments.cpp
 * Monuments 3D des cartes : modèles ponctuels posés à une coordonnée WGS84,
 * déclarés par carte dans monuments.txt et rangés sous
 * assets/models/monuments/<jeu>/. Ce fichier tient les deux bouts, le
 * chargement (loadMonuments, appelé par loadTerrain) et le dessin
 * (renderMonuments, appelé par renderFrame).
 *
 * Rien ici ne connaît Paris. Le mécanisme est celui de landmarks.txt et
 * hapi.txt : une carte pose ce qu'elle déclare, et la tour Eiffel n'est qu'une
 * ligne de données parmi d'autres. C'est ce qui permettra aux phares de
 * Capbreton de passer par le même chemin sans une ligne de code de plus.
 *
 * La lecture du fichier de déclaration vit avec les autres fichiers de la carte
 * (Terrain::loadMonuments, dans TerrainSetup.cpp) ; l'exclusion des emprises
 * BD TOPO sous un monument vit avec les bâtiments (BuildingsMesh.cpp).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/AppConstants.hpp"
#include "app/Application.hpp"

#include "render/Model.hpp"
#include "render/ModelLoader.hpp"
#include "render/Shader.hpp"
#include "render/Terrain.hpp"
#include "util/Math.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <utility>

namespace artouste::app {

std::filesystem::file_time_type
Application::dateMonuments(const std::filesystem::path& dossierCarte) const {
    std::error_code ec;
    const auto date = std::filesystem::last_write_time(dossierCarte / "monuments.txt", ec);
    return ec ? std::filesystem::file_time_type{} : date;
}

void Application::loadMonuments() {
    m_monuments.clear();
    if (!m_terrain) {
        return;
    }
    /* Date du fichier au moment du chargement : le retour au menu la comparera
       pour savoir s'il faut recharger la carte (voir applyMenuSession). */
    m_monumentsDate = dateMonuments(m_assetsDir / "terrain" / m_terrainName);

    const std::filesystem::path dir = m_assetsDir / "models" / "monuments";
    const float halfW = m_terrain->halfWidth();
    const float halfH = m_terrain->halfHeight();
    const float origX = m_terrain->originX();
    const float origZ = m_terrain->originZ();

    for (const render::Monument& mon : m_terrain->monuments()) {
        const std::filesystem::path file = dir / mon.file;
        if (!std::filesystem::exists(file)) {
            std::fprintf(stderr,
                         "[scène] monument \"%s\" : %s introuvable, ignoré.\n",
                         mon.name.c_str(),
                         file.string().c_str());
            continue;
        }

        /* Monument hors de l'emprise de la carte (bornes recadrées) : on ne le
           charge pas plutôt que de le laisser flotter au-delà du bord du relief. */
        float x = 0.0f, z = 0.0f;
        m_terrain->worldAt(mon.lon, mon.lat, x, z);
        if (std::fabs(x - origX) > halfW || std::fabs(z - origZ) > halfH) {
            std::printf("[scène] monument \"%s\" hors de l'emprise de la carte, ignoré.\n",
                        mon.name.c_str());
            continue;
        }

        render::Model model = render::ModelLoader::load(file, {}, {});
        if (model.empty()) {
            continue; /* ModelLoader a déjà dit pourquoi sur la sortie d'erreur */
        }

        const float y = mon.onGround ? m_terrain->heightAt(x, z) : mon.altitudeM;

        /* Recalage du modèle sur son propre repère, avant toute pose.

           En plan, on le recentre sur sa boîte englobante : les auteurs de scène
           ne posent pas tous l'origine au milieu de leur géométrie, celle de la
           tour Eiffel y est (à 16 cm près), celle du Sacré-Coeur en est à 70 m.
           Sans ce recalage, la coordonnée du fichier ne voudrait rien dire de
           commun d'un monument à l'autre, et il faudrait retrouver le décalage
           de chacun à la main. Recentré, lon/lat désigne toujours le milieu du
           monument, c'est-à-dire ce que donne l'emprise BD TOPO qui sert à le
           mesurer.

           En altitude, on ramène le POINT LE PLUS BAS à zéro, et non l'origine.
           Là encore les auteurs ne s'accordent pas : cinq de nos six modèles ont
           leur socle à quelques centimètres de zéro, celui de Notre-Dame est à
           +7,13 m et la cathédrale flottait d'autant. L'altitude du fichier
           (ou le relief sous le monument, mot-clé "sol") désigne donc où poser
           la BASE, ce qui est la seule lecture qui ait un sens. */
        vec3 mn{1e30f, 1e30f, 1e30f};
        vec3 mx{-1e30f, -1e30f, -1e30f};
        for (const vec3& p : model.positions()) {
            mn = glm::min(mn, p);
            mx = glm::max(mx, p);
        }
        const vec3 centre{0.5f * (mn.x + mx.x), mn.y, 0.5f * (mn.z + mx.z)};
        if (std::fabs(mn.y) > 1.0f) {
            std::printf("[scène] monument \"%s\" : socle du modèle à %+.2f m, ramené au sol.\n",
                        mon.name.c_str(),
                        static_cast<double>(mn.y));
        }

        /* Pose du modèle. Le cap est un cap boussole (0 = nord, 90 = est), donc
           une rotation dans le sens des aiguilles vue de dessus. Dans ce repère
           (X est, Y haut, Z sud), une rotation positive autour de Y amène X vers
           le nord, c'est-à-dire dans le sens inverse : on prend donc l'opposé du
           cap. Cap 0 laisse le modèle tel que son auteur l'a orienté. */
        MonumentInstance inst;
        inst.transform = glm::translate(mat4(1.0f), vec3{x, y, z}) *
                         glm::rotate(mat4(1.0f), glm::radians(-mon.headingDeg),
                                     vec3{0.0f, 1.0f, 0.0f}) *
                         glm::scale(mat4(1.0f), vec3{mon.scaleH, mon.scaleV, mon.scaleH}) *
                         glm::translate(mat4(1.0f), -centre);
        inst.model = std::make_unique<render::Model>(std::move(model));
        inst.name  = mon.name;
        std::printf("[scène] monument \"%s\" posé (%.5f %.5f, %.1f m, cap %.0f, "
                    "échelle h x%.3f v x%.3f).\n",
                    mon.name.c_str(),
                    static_cast<double>(mon.lon),
                    static_cast<double>(mon.lat),
                    static_cast<double>(y),
                    static_cast<double>(mon.headingDeg),
                    static_cast<double>(mon.scaleH),
                    static_cast<double>(mon.scaleV));
        m_monuments.push_back(std::move(inst));
    }
}

void Application::renderMonuments(const RenderContext& ctx) {
    /*
     * Dessinés après les bâtiments extrudés, dont ils ont fait dégager l'emprise au
     * moment de bâtir le maillage. Test alpha et écriture de profondeur (voir
     * monument.frag) : pas de mélange, donc l'ordre entre monuments n'a pas
     * d'importance.
     */
    if (m_monuments.empty() || !m_monumentShader) {
        return;
    }

    m_monumentShader->use();
    m_monumentShader->setMat4("u_view", ctx.view);
    m_monumentShader->setMat4("u_proj", ctx.proj);
    m_monumentShader->setVec3("u_lightDir", ctx.lightDir);
    m_monumentShader->setVec3("u_camPos", ctx.camPosRel);
    m_monumentShader->setVec3("u_fogColor", ctx.fogColor);
    m_monumentShader->setFloat("u_fogStart", FOG_START);
    m_monumentShader->setFloat("u_fogEnd", FOG_END);
    m_monumentShader->setInt("u_texture", 0);
    /* Seuil du test alpha : la moitié. Les textures des monuments FlightGear sont
       franches (18 % de pixels tout à fait transparents pour la tour Eiffel, 75 %
       tout à fait opaques) ; un seuil médian tranche donc dans la frange
       intermédiaire, sans ronger le treillis ni laisser de halo. */
    m_monumentShader->setFloat("u_alphaCutoff", 0.5f);

    /* Aucun réglage d'élimination des faces arrière ici. Le moteur n'y touche
       nulle part et la laisse donc éteinte, valeur par défaut d'OpenGL : tout
       est dessiné des deux côtés, ce qui convient aux monuments (les poutrelles
       de la tour Eiffel sont des panneaux plats dont on doit voir les deux
       faces) comme au reste. Une version antérieure la rallumait après la
       boucle et la laissait allumée pour le reste de l'image : les instruments
       de la planche de bord devenaient transparents, leurs faces arrière étant
       éliminées. */
    for (const MonumentInstance& mon : m_monuments) {
        /* La matrice de pose est en coordonnées monde absolues ; ctx.toRel porte le
           recalage d'origine de l'image (rendu relatif à la caméra). */
        m_monumentShader->setMat4("u_model", ctx.toRel * mon.transform);
        mon.model->draw(*m_monumentShader, render::Pass::All);
    }
}

} /* namespace artouste::app */
