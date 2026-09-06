/*
 * LoadedHelicopter.cpp
 * Construction de l'hélicoptère chargé : assemblage des pièces (fuselage,
 * intérieur, pilote et ses bras/jambes articulés, commandes, rotors, décalques)
 * à partir des fichiers FlightGear. Le chargement des instruments animés et le
 * dessin sont dans LoadedHelicopterInstruments.cpp et LoadedHelicopterDraw.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/LoadedHelicopter.hpp"

#include "render/LoadedHelicopterDetail.hpp"
#include "render/Shader.hpp"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace artouste::render {

using namespace heli_detail;

LoadedHelicopter::LoadedHelicopter(const std::filesystem::path& modelFile) {
    const std::filesystem::path dir = modelFile.parent_path();
    /* Listes de nœuds à écarter au chargement :
       - skipBody : plans flous des rotors (blur/disc), doublons HDR des vitrages,
         version flotteurs du train (on garde les patins, par défaut chez FlightGear),
         et les roues (roueD/roueG) que l'on ne veut pas afficher.
       - skipRotor : uniquement les plans flous des rotors. */
    const std::vector<std::string> skipBody{"hdr", "blur", "disc", "flotteur", "barre", "roue"};
    const std::vector<std::string> skipRotor{"blur", "disc"};
    /* Nœuds à rendre translucides : la verrière de cabine et les vitres de porte.
       On ne vise pas "tourvitre" : c'est un maillage qui mêle le vitrage avant ET la
       partie pleine au niveau du tableau de bord ; le laisser opaque évite que cette
       partie pleine devienne un trou transparent. (Le vrai correctif serait de scinder
       ce maillage dans le modèle pour ne rendre transparent que le verre.) */
    const std::vector<std::string> glass{"verriere", "vitreporte"};

    m_fuselage  = loadPart(modelFile, skipBody, glass);

    /* Modèle glTF (.glb/.gltf) : autre convention d'axes que les .ac FlightGear
       (nez vers +Z au lieu de -X) et bas de l'appareil à une autre hauteur. On le
       redresse au dessin, quart de tour puis descente pour reposer les patins sur
       le sol, sans toucher au fichier. */
    if (const std::string ext = modelFile.extension().string();
        ext == ".glb" || ext == ".gltf") {
        float minY = 0.0f;
        for (const vec3& p : m_fuselage.positions()) {
            minY = std::min(minY, p.y);
        }
        m_fuselageFix = glm::translate(mat4(1.0f), vec3{0.0f, -(Y_OFFSET + minY), 0.0f}) *
                        glm::rotate(mat4(1.0f), -PI / 2.0f, vec3{0.0f, 1.0f, 0.0f});
        std::printf("[modèle] glTF redressé : quart de tour, descente de %.2f m.\n",
                    static_cast<double>(Y_OFFSET + minY));
    }
    /* Livrées de rechange du fuselage : blanc, bleu Gendarmerie, olive armée de
       terre et rouge Protection civile, préchargées dans le cache du fuselage et
       activables à la demande (setLivery). */
    m_liveryBlanche          = m_fuselage.acquireTexture(dir / "texture-blanche.png");
    m_liveryGendarmerie      = m_fuselage.acquireTexture(dir / "texture-gendarmerie.png");
    m_liveryArmeeDeTerre     = m_fuselage.acquireTexture(dir / "texture-armeedeterre.png");
    m_liveryProtectionCivile = m_fuselage.acquireTexture(dir / "texture-protectioncivile.png");
    m_interior  = loadPart(dir / "Interior/interior.ac", skipBody, glass);
    /* Intérieur de cabine : version assombrie par l'occlusion ambiante cuite
       (voir tools/livree/make_interior_ao.py), posée comme texture de rechange
       pour laisser intacte la texture d'origine du modèle FlightGear. Fichier
       absent : acquireTexture renvoie nullptr et l'original reste en place. */
    m_interior.setLivery(m_interior.acquireTexture(dir / "Interior/interior-occlusion.png"));

    chargerPilote(dir);

    /* Palonnier : pédales gauche (paloG) et droite (paloD) isolées, pour les faire
       basculer en sens opposé au palonnier. */
    m_pedalLeft  = loadPart(dir / "Interior/Panel/Instruments/pedals/pedals.ac", {"palod"});
    m_pedalRight = loadPart(dir / "Interior/Panel/Instruments/pedals/pedals.ac", {"palog"});
    /* Manche cyclique : la colonne complète, recopiée devant chaque siège. */
    m_cyclic = loadPart(dir / "Interior/Panel/Instruments/yokes/yoke.ac", skipBody);

    /* Levier de collectif, chargé en deux morceaux pour pouvoir animer le levier
       (qui pivote) sans bouger son embase (fixée au plancher). */
    const std::filesystem::path collectivePath =
        dir / "Interior/Panel/Instruments/collective/collective.ac";
    m_collectiveBase  = loadPart(collectivePath, {"collective"});  /* ne garde que l'embase */
    m_collectiveLever = loadPart(collectivePath, {"base"});        /* ne garde que le levier */
    /* Poignée = bout du levier le plus en avant (x le plus négatif dans son repère),
       là où la main gauche vient se poser. C'est bien le pommeau qui est saisi : c'est
       la PAUME (et non le bout des doigts) qui y est ancrée, voir m_handLeftLocal. */
    {
        float minX = 1e30f;
        for (const vec3& p : m_collectiveLever.positions()) {
            if (p.x < minX) {
                minX = p.x;
                m_collectiveGripLocal = p;
            }
        }
    }

    /* Planche de bord : on écarte aussi les capots (sur1..sur6) posés au-dessus
       de chaque cadran. Sans reflet de verre pour les adoucir, ces pare-soleil
       sombres ressortent comme de vilains rectangles noirs devant les
       instruments une fois ceux-ci bien visibles. */
    const std::vector<std::string> skipPanel{"blur", "disc", "sur"};
    m_panel = loadPart(dir / "Interior/Panel/panel.ac", skipPanel);
    /* Planche de bord assombrie par l'occlusion ambiante, même principe que
       l'intérieur : texture de rechange, l'originale reste intacte. */
    m_panel.setLivery(m_panel.acquireTexture(dir / "Interior/Panel/panel-occlusion.png"));
    for (const GaugeDef& def : GAUGES) {
        Gauge gauge;
        gauge.model  = loadPart(dir / def.file, {"blur", "disc", "vitre"});
        gauge.offset = PANEL_OFFSET + fgToAssimp(def.fgOffset);
        m_gauges.push_back(std::move(gauge));
    }

    /* Instruments animés du tableau de bord (chargés à part, voir
       LoadedHelicopterInstruments.cpp). */
    loadInstruments(dir);

    /* Pièces des rotors : moyeu et pale, principaux et de queue. Une seule pale
       est chargée par rotor, puis recopiée et tournée à l'affichage. */
    m_mainHub   = loadPart(dir / "Externals/MainRotor/mainrotor.ac", skipRotor);
    m_mainBlade = loadPart(dir / "Externals/MainRotor/blade.ac", skipRotor);
    m_tailHub   = loadPart(dir / "Externals/TailRotor/tailrotor.ac", skipRotor);
    m_tailBlade = loadPart(dir / "Externals/TailRotor/blade.ac", skipRotor);
    /* Plans flous du modèle : ils sont dans blade.ac, à côté de la pale nette. On
       recharge donc le fichier deux fois en écartant ce qu'on ne veut pas. Le
       propblur (pale élargie) se pose sur chaque pale, le propdisc (secteur de
       disque) se répète tout autour du mât, d'où deux pièces séparées. */
    m_mainBlur = loadPart(dir / "Externals/MainRotor/blade.ac", {"blade", "disc"});
    m_mainDisc = loadPart(dir / "Externals/MainRotor/blade.ac", {"blade", "blur"});
    m_tailBlur = loadPart(dir / "Externals/TailRotor/blade.ac", {"blade", "disc"});
    m_tailDisc = loadPart(dir / "Externals/TailRotor/blade.ac", {"blade", "blur"});
    /* Livrée Gendarmerie des pales de queue : texture de rechange (jaune zébré
       rouge), préchargée dans le cache de la pale et activée à la demande. */
    m_tailBladeLivery =
        m_tailBlade.acquireTexture(dir / "Externals/TailRotor/tailrotor-gendarmerie.png");

    /* Arceau de protection du rotor de queue : isolé de la structure du fuselage
       (voir tools/livree) pour pouvoir le peindre en jaune en livrée Gendarmerie.
       Le fuselage lui-même (alouette.ac) ne le contient donc plus. */
    m_tailGuard        = loadPart(dir / "tailguard.ac", {});
    m_tailGuardLivery  = m_tailGuard.acquireTexture(dir / "tailguard-gendarmerie.png");
    m_tailGuardOrigine = m_tailGuard.acquireTexture(dir / "tailguard-origine.png");

    /* Marquages de livrée (posés sur les flancs en 3D, voir draw) : ceux de la
       Gendarmerie et le code d'appareil "341-HN" de la livrée armée de terre. */
    m_decalGendarmerie = makeDecal(dir / "decal-gendarmerie.png");
    m_decalReg         = makeDecal(dir / "decal-fbrhp.png");
    m_decalStripe      = makeDecal(dir / "decal-stripe.png");
    m_decalReg341      = makeDecal(dir / "decal-341hn.png");
    m_decalProtCiv     = makeDecal(dir / "decal-protectioncivile.png");
    m_decalRegAyem     = makeDecal(dir / "decal-fayem.png");
}

void LoadedHelicopter::loadPilotSkin(const std::filesystem::path& pilotDir, Model& model) {
    PilotSkin skin;
    skin.model            = &model;
    skin.gendarmerie      = model.acquireTexture(pilotDir / "general_pilot-gendarmerie.png");
    skin.armeeDeTerre     = model.acquireTexture(pilotDir / "general_pilot-armeedeterre.png");
    skin.protectionCivile = model.acquireTexture(pilotDir / "general_pilot-protectioncivile.png");
    m_pilotSkins.push_back(skin);
}

void LoadedHelicopter::setLivery(Livery livery) {
    m_livery = livery;

    /* Fuselage : texture de rechange selon la livrée (nullptr = texture d'origine). */
    const Texture* fuselageTex = nullptr;
    switch (livery) {
        case Livery::Gendarmerie:      fuselageTex = m_liveryGendarmerie;      break;
        case Livery::ArmeeDeTerre:     fuselageTex = m_liveryArmeeDeTerre;     break;
        case Livery::ProtectionCivile: fuselageTex = m_liveryProtectionCivile; break;
        case Livery::Blanche:          fuselageTex = m_liveryBlanche;          break;
    }
    m_fuselage.setLivery(fuselageTex);

    /* Tenue du pilote (chemise et casque) : bleu Gendarmerie, olive armée de
       terre, orange Protection civile ; la livrée blanche garde la tenue
       civile d'origine (nullptr = texture d'origine, comme le fuselage). */
    for (const PilotSkin& skin : m_pilotSkins) {
        const Texture* pilotTex = nullptr;
        switch (livery) {
            case Livery::Gendarmerie:      pilotTex = skin.gendarmerie;      break;
            case Livery::ArmeeDeTerre:     pilotTex = skin.armeeDeTerre;     break;
            case Livery::ProtectionCivile: pilotTex = skin.protectionCivile; break;
            case Livery::Blanche:          pilotTex = nullptr;               break;
        }
        skin.model->setLivery(pilotTex);
    }

    /* Pales de queue : zébrées jaune/rouge sur TOUTES les livrées. C'est un repère
       de sécurité du rotor de queue (très visible), qu'on conserve quelle que soit
       la livrée plutôt que de le réserver à la Gendarmerie. */
    m_tailBlade.setLivery(m_tailBladeLivery);

    /* Arceau de protection : jaune uni sur toutes les livrées, cohérent avec les
       pales de queue zébrées. (Sa texture d'origine n'a pas de teinte satisfaisante,
       UV chaudes ; on force donc la couleur unie.) */
    m_tailGuard.setLivery(m_tailGuardLivery);
}

void LoadedHelicopter::drawModel(Shader& shader, const Model& model, const mat4& transform,
                                 Pass pass, float opacityScale) const {
    shader.setMat4("u_model", transform);
    model.draw(shader, pass, opacityScale);
}

}  /* namespace artouste::render */
