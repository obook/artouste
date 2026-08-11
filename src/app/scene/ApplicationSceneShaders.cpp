/*
 * ApplicationSceneShaders.cpp
 * Chargement des shaders, des maillages procéduraux (disque d'ombre, sphère
 * de lueur, hélipad, jupe) et des modèles associés (zombies, projectiles,
 * explosions, hélipad texturé), plus les textures de détail et de façade.
 * Appelée par initScene (ApplicationScene.cpp) avant initSceneConfig
 * (ApplicationSceneConfig.cpp).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/AppConstants.hpp"
#include "app/Application.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Mesh.hpp"
#include "render/Model.hpp"
#include "render/ModelLoader.hpp"
#include "render/Primitives.hpp"
#include "render/Shader.hpp"
#include "render/Skybox.hpp"
#include "render/SouffleFx.hpp"
#include "render/Texture.hpp"
#include "render/combat/ExplosionFx.hpp"
#include "render/combat/Projectiles.hpp"
#include "render/combat/SkinnedZombies.hpp"
#include "render/combat/ZombieEyes.hpp"

#include <filesystem>
#include <vector>

namespace artouste::app {

namespace {

/*
 * Capacité du tampon d'instances du mode zombie (nombre maximal de zombies
 * dessinés simultanément, voir render::SkinnedZombies). Dimensionnée large
 * pour les manches tardives (gestionnaire de vagues) ; le point de spawn
 * initial n'en utilise qu'une poignée (zombies.txt).
 */
constexpr std::size_t ZOMBIE_CAPACITY = 300;

/*
 * Nombre de groupes de phase de marche (rendu skinné) : l'animation est posée à
 * autant d'instants déphasés, chaque zombie étant rattaché à un groupe, pour une
 * marche désynchronisée à coût maîtrisé (voir render::SkinnedZombies). Six suffit
 * à casser l'effet "tous au même pas" sans multiplier les dessins.
 */
constexpr int ZOMBIE_PHASE_GROUPS = 6;

/*
 * Capacité du tampon d'instances des boulettes toxiques (voir
 * app::ProjectileSystem::MAX_PROJECTILES, même valeur -- inutile de réserver
 * plus côté GPU que ce que la logique de jeu peut produire à la fois).
 */
constexpr std::size_t PROJECTILE_CAPACITY = 64;

/*
 * Capacité du tampon des lueurs d'yeux : deux par zombie dessiné, donc le
 * double de ZOMBIE_CAPACITY (voir render::combat::ZombieEyes).
 */
constexpr std::size_t ZOMBIE_EYES_CAPACITY = ZOMBIE_CAPACITY * 2;

/*
 * Nombre maximal de bouffées de poussière vivantes sous l'appareil (souffle
 * rotor). C'est le seul plafond de coût de l'effet : 600 billboards translucides
 * de quelques mètres, dessinés en un appel, restent négligeables devant la
 * végétation. La simulation et le tampon GPU partagent cette valeur, le second
 * ne devant jamais être plus petit que la première.
 */
constexpr std::size_t SOUFFLE_CAPACITY = 600;

} /* namespace */

void Application::initSceneShaders() {
    const std::filesystem::path& assets = m_assetsDir;

    m_shader = std::make_unique<render::Shader>(assets / "shaders" / "basic.vert",
                                                assets / "shaders" / "basic.frag");
    m_modelShader = std::make_unique<render::Shader>(assets / "shaders" / "model.vert",
                                                     assets / "shaders" / "model.frag");
    /* Unité de texture de la carte de relief, posée une fois pour toutes : la
       valeur d'un uniforme appartient au programme, inutile de la redonner à
       chaque appel de dessin (la texture de couleur reste sur l'unité 0). */
    m_modelShader->use();
    m_modelShader->setInt("u_relief", 1);
    m_terrainShader = std::make_unique<render::Shader>(assets / "shaders" / "terrain.vert",
                                                       assets / "shaders" / "terrain.frag");
    m_seaShader = std::make_unique<render::Shader>(assets / "shaders" / "sea.vert",
                                                   assets / "shaders" / "sea.frag");
    m_skyShader = std::make_unique<render::Shader>(assets / "shaders" / "sky.vert",
                                                   assets / "shaders" / "sky.frag");
    m_flatShader = std::make_unique<render::Shader>(assets / "shaders" / "flat.vert",
                                                    assets / "shaders" / "flat.frag");
    m_shadowShader = std::make_unique<render::Shader>(assets / "shaders" / "shadow.vert",
                                                      assets / "shaders" / "shadow.frag");
    m_buildingShader = std::make_unique<render::Shader>(assets / "shaders" / "building.vert",
                                                        assets / "shaders" / "building.frag");
    m_monumentShader = std::make_unique<render::Shader>(assets / "shaders" / "monument.vert",
                                                        assets / "shaders" / "monument.frag");
    m_vegetationShader = std::make_unique<render::Shader>(assets / "shaders" / "vegetation.vert",
                                                          assets / "shaders" / "vegetation.frag");
    m_cloudShader = std::make_unique<render::Shader>(assets / "shaders" / "clouds.vert",
                                                     assets / "shaders" / "clouds.frag");
    m_zombieShader = std::make_unique<render::Shader>(assets / "shaders" / "zombie_skinned.vert",
                                                      assets / "shaders" / "zombie_skinned.frag");
    m_projectileShader = std::make_unique<render::Shader>(assets / "shaders" / "projectile.vert",
                                                          assets / "shaders" / "projectile.frag");
    m_explosionShader = std::make_unique<render::Shader>(assets / "shaders" / "explosion.vert",
                                                         assets / "shaders" / "explosion.frag");
    m_zombieEyesShader = std::make_unique<render::Shader>(assets / "shaders" / "zombie_eyes.vert",
                                                          assets / "shaders" / "zombie_eyes.frag");
    m_souffleShader = std::make_unique<render::Shader>(assets / "shaders" / "souffle.vert",
                                                       assets / "shaders" / "souffle.frag");
    m_sky = std::make_unique<render::Skybox>();

    /* Souffle rotor : poussière soulevée près du sol, toujours active. Le rayon
       du rotor vient du modèle de l'appareil : c'est lui qui
       fixe l'anneau d'émission au sol et la hauteur au-delà de laquelle le
       souffle ne soulève plus rien. */
    m_souffle = SouffleRotor(render::LoadedHelicopter::MAIN_ROTOR_RADIUS, SOUFFLE_CAPACITY);
    m_souffleFx = std::make_unique<render::SouffleFx>(SOUFFLE_CAPACITY);

    /* Mode zombie : pack de personnages skinnés (marche + bras animés) chargé une
       seule fois (indépendant de la carte), voir CREDITS.md pour l'attribution.
       Absent : m_zombiesRender reste nul, aucun zombie ne sera dessiné (CombatMode
       peut malgré tout s'activer sur les cartes compatibles, sans effet visuel). */
    const std::filesystem::path zombieModel = assets / "models" / "zombie" / "zombies_animated.glb";
    if (std::filesystem::exists(zombieModel)) {
        m_zombiesRender = std::make_unique<render::SkinnedZombies>(
            zombieModel, ZOMBIE_CAPACITY, ZOMBIE_PHASE_GROUPS);
    }
    /* Boulettes toxiques : billboard procédural, pas de modèle à charger. */
    m_projectilesRender = std::make_unique<render::Projectiles>(PROJECTILE_CAPACITY);

    /* Lueur des yeux : billboard procédural lui aussi, indépendant du pack
       skinné (elle est posée sur la tête du modèle, voir ZombieHorde::buildEyes)
       et donc dessinée même si ce pack manque. */
    m_zombieEyesRender = std::make_unique<render::ZombieEyes>(ZOMBIE_EYES_CAPACITY);

    /* Explosions 3D des roquettes : modèle animé chargé une fois. Rayon monde
       proche de la zone létale (RocketSystem::EXPLOSION_RADIUS_M = 3 m). Absent :
       m_explosionFx->built() reste faux, aucune explosion 3D dessinée. */
    const std::filesystem::path explosionModel = assets / "models" / "zombie" / "explosion.glb";
    if (std::filesystem::exists(explosionModel)) {
        /* Rayon monde 3,5 m (proche de la zone létale). Le clip (12 images figées
           enchainees, 3,0 s au total -- voir le diagnostic [ExplosionModel] au
           chargement) n'affiche nativement que 4 images/s : saccadé si on ne
           montre qu'une tranche de 1,2 s (RocketSystem::EXPLOSION_DURATION_S,
           la vie réelle de l'explosion) a vitesse native, comme avant (5 images
           vues sur 12). Départ d'anim a 0,3 s (saute juste la toute première
           image, encore informe) puis lecture accélérée x2,25 pour atteindre la
           fin du clip (3,0 s) en 1,2 s de vie réelle : les ~11 images restantes
           défilent dans le même temps de vie, sensiblement moins saccadé, sans
           toucher au fichier ni a ses images (photos de feu réalistes). */
        m_explosionFx =
            std::make_unique<render::ExplosionFx>(explosionModel, 3.5f, 0.3f, 1.2f, 2.25f);
    }

    const auto discData = render::primitives::softDisc(6.0f, 48);
    m_shadowDisc = std::make_unique<render::Mesh>(discData.vertices, discData.indices);

    /* Petite sphère unité, réutilisée (mise à l'échelle au dessin) pour le flash du
       strombo et la lueur de la tuyère. Le shader plat ne lit que la position. */
    const auto sphereData = render::primitives::sphere(1.0f, 12, 16, vec3{1.0f, 1.0f, 1.0f});
    m_glowSphere = std::make_unique<render::Mesh>(sphereData.vertices, sphereData.indices);

    /* Hélipad de la zone de départ : disque béton foncé, anneau et grand H blancs
       (marquage d'hélistation civile, sans croix). Centré sur l'origine ; placé au
       départ à l'affichage. Repli seulement : la version texturée le remplace si elle
       est présente. */
    const auto padData = render::primitives::helipad(
        7.0f, 48, vec3{0.45f, 0.45f, 0.47f}, vec3{0.92f, 0.92f, 0.90f}, vec3{0.95f, 0.95f, 0.93f});
    m_helipad = std::make_unique<render::Mesh>(padData.vertices, padData.indices);

    /* Jupe des hélisurfaces : paroi cylindrique sous le disque, pour les pads
       perchés dont le plateau surplombe le relief (sommet du pic du Midi
       d'Ossau). Gris béton dégradé vers l'ombre ; la partie enterrée est
       simplement cachée par le test de profondeur. */
    const auto skirtData = render::primitives::tube(
        6.9f, 10.0f, 48, vec3{0.52f, 0.52f, 0.54f}, vec3{0.26f, 0.26f, 0.28f});
    m_padSkirt = std::make_unique<render::Mesh>(skirtData.vertices, skirtData.indices);

    /* Hélipad texturé fabriqué avec Blender (voir tools/helipad). S'il est présent,
       il remplace la version procédurale ci-dessus ; sinon on garde celle-ci. */
    const std::filesystem::path helipadModel = assets / "models" / "helipad" / "helipad.ac";
    if (std::filesystem::exists(helipadModel)) {
        render::Model pad = render::ModelLoader::load(helipadModel, {}, {});
        if (!pad.empty()) {
            m_helipadModel = std::make_unique<render::Model>(std::move(pad));
        }
    }

    /* Grain rocheux du terrain : texture de détail tuilable mélangée à l'ortho
       de près par terrain.frag (voir u_detail). Absente : le shader reçoit une
       unité vide, mais le fichier fait partie du dépôt. */
    const std::filesystem::path detailPath = assets / "textures" / "detail-roche.png";
    if (std::filesystem::exists(detailPath)) {
        m_terrainDetail = std::make_unique<render::Texture>(detailPath);
    }

    /* Façade tuilée des bâtiments (fenêtres), voir u_facade dans building.frag.
       Absente : le shader reçoit une unité vide, les murs restent en couleur unie. */
    const std::filesystem::path facadePath = assets / "textures" / "facade.png";
    if (std::filesystem::exists(facadePath)) {
        m_buildingFacade = std::make_unique<render::Texture>(facadePath);
    }

    /* Plan de mer : un grand quadrilatère horizontal qui s'étend jusqu'à l'horizon. */
    const vec3 up{0.0f, 1.0f, 0.0f};
    const std::vector<render::Vertex> seaVerts = {
        {{-SEA_HALF, SEA_LEVEL, -SEA_HALF}, up, SEA_COLOR, {0.0f, 0.0f}},
        {{SEA_HALF, SEA_LEVEL, -SEA_HALF}, up, SEA_COLOR, {0.0f, 0.0f}},
        {{SEA_HALF, SEA_LEVEL, SEA_HALF}, up, SEA_COLOR, {0.0f, 0.0f}},
        {{-SEA_HALF, SEA_LEVEL, SEA_HALF}, up, SEA_COLOR, {0.0f, 0.0f}},
    };
    const std::vector<unsigned int> seaIdx = {0, 1, 2, 0, 2, 3};
    m_sea = std::make_unique<render::Mesh>(seaVerts, seaIdx);
}

} /* namespace artouste::app */
