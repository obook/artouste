/*
 * LoadedHelicopterPlanche.cpp
 * Cadrans dessinés de la planche du poste 3dexport (voir PlancheBord.hpp) :
 * leurs faces sont peintes dans l'atlas intérieur, sans aiguilles. On pose ici
 * les aiguilles : une même lame blanche, mise à la longueur et à l'angle voulus.
 * Le compas magnétique du FBX a une fenêtre peinte : on pose devant une bande
 * graduée (tools/modele/rose_compas.py) qui défile avec le cap.
 *
 * Auteur : O. Booklage
 * Date : novembre 2026
 * Licence : GPL v2
 */

#include "render/LoadedHelicopter.hpp"

#include "render/LoadedHelicopterDetail.hpp"
#include "render/PlacementCadran.hpp"
#include "render/PlancheBord.hpp"
#include "render/Shader.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace artouste::render {

using namespace heli_detail;

namespace {

/* PNG blanc d'un pixel : texture unie des aiguilles. */
const unsigned char BLANC_PNG[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48,
    0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00,
    0x00, 0x90, 0x77, 0x53, 0xde, 0x00, 0x00, 0x00, 0x0c, 0x49, 0x44, 0x41, 0x54, 0x78,
    0xda, 0x63, 0xf8, 0xff, 0xff, 0x3f, 0x00, 0x05, 0xfe, 0x02, 0xfe, 0x33, 0x12, 0x95,
    0x14, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

/* PNG presque noir d'un pixel : la bille de l'indicateur de virage. */
const unsigned char NOIR_PNG[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48,
    0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00,
    0x00, 0x90, 0x77, 0x53, 0xde, 0x00, 0x00, 0x00, 0x0c, 0x49, 0x44, 0x41, 0x54, 0x78,
    0xda, 0x63, 0xe0, 0xe1, 0xe1, 0x01, 0x00, 0x00, 0x4c, 0x00, 0x25, 0x96, 0x99, 0x64,
    0xb8, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

/* Voyants de la console : lentilles mesurées sur le .glb (repère racine),
   de gauche à droite vus du pilote. Chacun reçoit une coupole (demi-sphère
   aplatie) texturée par voyants.png (tools/modele/voyants.py) : bande de sa
   couleur allumée ou éteinte. */
const vec3      VOYANTS_NORMALE{0.950f, 0.310f, 0.0f};
constexpr float VOYANT_X       = -4.3055f;
constexpr float VOYANT_Y       = -0.4495f;
constexpr float VOYANT_RAYON   = 0.0086f;
constexpr float VOYANT_DEVANT  = 0.0006f;  /* lentille bombée de 0,6 mm */
constexpr float VOYANT_HAUTEUR = 0.6f;     /* hauteur de la coupole, en rayons */
/* Bandes de voyants.png, dans l'ordre du générateur. */
enum Bande { ROUGE_ALLUME, VERT_ALLUME, AMBRE_ALLUME, ROUGE_ETEINT, VERT_ETEINT, AMBRE_ETEINT, NB_BANDES };
constexpr float VOYANT_RELAIS_Z      = 0.0942f;
constexpr float VOYANT_DEMARREUR_Z   = 0.0594f;
constexpr float VOYANT_MICROPOMPE_Z  = 0.0348f;
constexpr float VOYANT_GENERATRICE_Z = 0.0082f;
constexpr float VOYANT_HUILE_BTP_Z   = -0.0462f;

/* Cadran dessiné : centre (cercle ajusté sur le bord) et normale de sa face
   plate mesurés sur le .glb (repère racine), rayon de la face redessinée
   (90 % du disque, comme dans interieur_4096.py), puis décalage de la face
   vers la droite et le haut du pilote.
   Ce décalage vient du relief : chaque cadran est un cylindre qui dépasse de
   6 mm de la planche. De l'oeil du pilote, on voit son flanc du côté de
   l'oeil, ce qui décentre la face dans ce qu'on perçoit comme la lunette. La
   face glisse donc de la moitié de ce flanc apparent, (profondeur / 2) x tan
   de l'angle de vue, calculé pour chaque cadran depuis COCKPIT_EYE_POSTE_FBX.
   Mêmes valeurs dans interieur_4096.py. */
struct CadranDessine {
    vec3  centre;
    vec3  normale;
    float rayon;
    float droite;
    float haut;
};

const CadranDessine TACHYMETRE{{-4.4146f, -0.1516f, 0.1411f}, {0.948f, 0.317f, -0.004f}, 0.0356f,
                               0.0020f, 0.0003f};
const CadranDessine UNITE_TRIPLE{{-4.3789f, -0.2567f, 0.1370f}, {0.948f, 0.319f, -0.002f}, 0.0390f,
                                 0.0021f, 0.0008f};
const CadranDessine PAS_COLLECTIF{{-4.3787f, -0.2569f, -0.0871f}, {0.947f, 0.321f, 0.001f}, 0.0377f,
                                  0.0012f, 0.0007f};
const CadranDessine VOLTMETRE{{-4.3306f, -0.3988f, 0.0735f}, {0.948f, 0.318f, 0.002f}, 0.0281f,
                              0.0019f, 0.0014f};
const CadranDessine CARBURANT{{-4.3306f, -0.3988f, -0.0778f}, {0.948f, 0.318f, 0.002f}, 0.0278f,
                              0.0012f, 0.0014f};
const CadranDessine SERVO{{-4.3582f, -0.3684f, 0.1585f}, {0.945f, 0.328f, 0.005f}, 0.0203f,
                          0.0016f, 0.0009f};
/* Montre agrandie à 97 % du disque (le décalage la fait déborder d'un mm en
   haut à droite, sur le flanc). */
const CadranDessine MONTRE{{-4.3397f, -0.3719f, -0.0058f}, {0.946f, 0.323f, 0.0f}, 0.0294f,
                           0.0015f, 0.0013f};
/* Les deux petits cadrans à gauche du capot, côté copilote. */
const CadranDessine VIRAGE{{-4.3826f, -0.2481f, 0.2884f}, {0.945f, 0.326f, -0.003f}, 0.0198f,
                           0.0022f, 0.0006f};
const CadranDessine AIR{{-4.3826f, -0.2481f, 0.3535f}, {0.945f, 0.326f, -0.003f}, 0.0197f,
                        0.0024f, 0.0006f};

/* Bille de l'indicateur de virage : elle roule dans un tube courbe, arc de
   rayon BILLE_ARC centré BILLE_CENTRE au-dessus du centre du cadran (en
   rayons de cadran, comme le tube peint par interieur_4096.py), jusqu'à
   BILLE_BUTEE_DEG de chaque côté ; BILLE_DEG_PAR_G l'écarte selon la force
   latérale. */
constexpr float BILLE_CENTRE     = 0.35f;
constexpr float BILLE_ARC        = 0.85f;
constexpr float BILLE_RAYON      = 0.09f;
constexpr float BILLE_BUTEE_DEG  = 25.0f;
constexpr float BILLE_DEG_PAR_G  = 125.0f;

/* Les aiguilles passent 1,5 mm devant la face, chacune un peu devant la
   précédente pour ne pas scintiller là où elles se croisent. */
constexpr float DEVANT_FACE     = 0.0015f;
constexpr float ECART_AIGUILLES = 0.0003f;

/* Repère d'un cadran dessiné : origine au centre de sa face, décalage
   compris ; X vers le pilote, Y vers midi, droite du pilote vers -Z. */
mat4 repere(const CadranDessine& c) {
    return placementCadran(c.centre, c.normale, 1.0f, DEVANT_FACE) *
           glm::translate(mat4(1.0f), vec3{0.0f, c.haut, -c.droite});
}

/* Lame d'aiguille dans le plan du cadran : longueur 1 vers midi, base de
   largeur 1, pointe effilée, petite queue derrière le pivot. */
/* Fenêtre du compas magnétique, au-dessus du capot : centre et normale mesurés
   sur le FBX (repère racine). La rose passe 1,5 mm devant la fenêtre peinte,
   encore derrière le rebord du boîtier (4 mm). */
const vec3      COMPAS_CENTRE{-4.3795f, 0.085f, -0.0075f};
const vec3      COMPAS_NORMALE{0.9976f, 0.0698f, 0.0f};
constexpr float COMPAS_LARGEUR = 0.030f;
constexpr float COMPAS_HAUTEUR = 0.016f;
/* Part de la bande visible : même proportion que l'image (2048 x 256 pour
   un tour), pour que les chiffres ne soient pas étirés. */
constexpr float COMPAS_PART = COMPAS_LARGEUR / COMPAS_HAUTEUR * 256.0f / 2048.0f;

/* Carte de la rose : quad dans le plan de la fenêtre, u croissant vers la
   droite du pilote (-Z), centré sur u = 0 ; le défilement se fait au dessin. */
Model roseCompas(const Texture* texture, Model&& m) {
    std::vector<Vertex> v(4);
    const float l = COMPAS_LARGEUR * 0.5f;
    const float h = COMPAS_HAUTEUR * 0.5f;
    const float u = COMPAS_PART * 0.5f;
    v[0].position = vec3{0.0f, -h, l};  v[0].uv = vec2{-u, 0.0f};
    v[1].position = vec3{0.0f, -h, -l}; v[1].uv = vec2{u, 0.0f};
    v[2].position = vec3{0.0f, h, -l};  v[2].uv = vec2{u, 1.0f};
    v[3].position = vec3{0.0f, h, l};   v[3].uv = vec2{-u, 1.0f};
    for (Vertex& s : v) {
        s.normal = vec3{1.0f, 0.0f, 0.0f};
        s.color  = vec3{1.0f};
    }
    const std::vector<unsigned int> idx{0, 1, 2, 0, 2, 3};
    m.addPart(Mesh(v, idx), texture);
    return std::move(m);
}

/* Coupole de voyant : demi-sphère de rayon 1, aplatie à VOYANT_HAUTEUR le
   long de X (vers le pilote). u va du sommet (0) au bord (1), v est le
   milieu de la bande 'bande' de voyants.png (image retournée au chargement :
   la première bande est en haut, v proche de 1). La pièce est ajoutée à 'm',
   qui garde la texture dans son cache. */
void ajouterCoupole(Model& m, const Texture* texture, int bande) {
    constexpr unsigned int AZ = 24;  /* méridiens */
    constexpr unsigned int CE = 6;   /* cercles, du sommet au bord */
    const float            v  = 1.0f - (static_cast<float>(bande) + 0.5f) / static_cast<float>(NB_BANDES);
    std::vector<Vertex>    sommets;
    sommets.reserve(1 + AZ * CE);
    Vertex haut;
    haut.position = vec3{VOYANT_HAUTEUR, 0.0f, 0.0f};
    haut.normal   = vec3{1.0f, 0.0f, 0.0f};
    haut.color    = vec3{1.0f};
    haut.uv       = vec2{0.0f, v};
    sommets.push_back(haut);
    for (unsigned int c = 1; c <= CE; ++c) {
        const float t = HALF_PI * static_cast<float>(c) / static_cast<float>(CE);
        for (unsigned int a = 0; a < AZ; ++a) {
            const float phi = TWO_PI * static_cast<float>(a) / static_cast<float>(AZ);
            Vertex      s;
            s.position = vec3{VOYANT_HAUTEUR * std::cos(t), std::sin(t) * std::cos(phi),
                              std::sin(t) * std::sin(phi)};
            /* Normale de l'ellipsoïde aplati. */
            s.normal = glm::normalize(vec3{std::cos(t) / VOYANT_HAUTEUR,
                                           std::sin(t) * std::cos(phi),
                                           std::sin(t) * std::sin(phi)});
            s.color = vec3{1.0f};
            s.uv    = vec2{static_cast<float>(c) / static_cast<float>(CE), v};
            sommets.push_back(s);
        }
    }
    std::vector<unsigned int> idx;
    for (unsigned int a = 0; a < AZ; ++a) {
        idx.insert(idx.end(), {0u, 1 + a, 1 + (a + 1) % AZ});
    }
    for (unsigned int c = 1; c < CE; ++c) {
        const unsigned int r0 = 1 + (c - 1) * AZ;
        const unsigned int r1 = 1 + c * AZ;
        for (unsigned int a = 0; a < AZ; ++a) {
            const unsigned int b = (a + 1) % AZ;
            idx.insert(idx.end(), {r0 + a, r1 + a, r1 + b, r0 + a, r1 + b, r0 + b});
        }
    }
    m.addPart(Mesh(sommets, idx), texture);
}

/* Disque de rayon 1 dans le plan d'un cadran, de la couleur du PNG d'un
   pixel donné : la bille de l'indicateur de virage. */
Model disque(const char* nom, const unsigned char* png, std::size_t taille) {
    constexpr unsigned int N = 16;
    std::vector<Vertex>    v(N + 1);
    v[0].position = vec3{0.0f};
    for (unsigned int i = 0; i < N; ++i) {
        const float a     = TWO_PI * static_cast<float>(i) / static_cast<float>(N);
        v[i + 1].position = vec3{0.0f, std::cos(a), std::sin(a)};
    }
    std::vector<unsigned int> idx;
    for (unsigned int i = 0; i < N; ++i) {
        idx.insert(idx.end(), {0u, i + 1, (i + 1) % N + 1});
    }
    for (Vertex& s : v) {
        s.normal = vec3{1.0f, 0.0f, 0.0f};
        s.color  = vec3{1.0f};
        s.uv     = vec2{0.5f};
    }
    Model m;
    m.addPart(Mesh(v, idx), m.acquireEmbeddedTexture(nom, png, taille));
    return m;
}

Model aiguilleUnite() {
    std::vector<Vertex> v(4);
    v[0].position = vec3{0.0f, -0.15f, -0.5f};
    v[1].position = vec3{0.0f, -0.15f, 0.5f};
    v[2].position = vec3{0.0f, 1.0f, 0.15f};
    v[3].position = vec3{0.0f, 1.0f, -0.15f};
    for (Vertex& s : v) {
        s.normal = vec3{1.0f, 0.0f, 0.0f};
        s.color  = vec3{1.0f};
        s.uv     = vec2{0.5f};
    }
    const std::vector<unsigned int> idx{0, 1, 2, 0, 2, 3};
    Model m;
    m.addPart(Mesh(v, idx), m.acquireEmbeddedTexture("blanc", BLANC_PNG, sizeof(BLANC_PNG)));
    return m;
}

}  /* namespace */

void LoadedHelicopter::chargerPlancheGltf(const std::filesystem::path& dir) {
    m_aiguille = aiguilleUnite();
    m_bille    = disque("noir", NOIR_PNG, sizeof(NOIR_PNG));
    /* Coupoles des voyants : une par bande de voyants.png. Chacune charge la
       petite texture (une Model ne partage pas son cache). */
    m_voyants.clear();
    for (int b = 0; b < NB_BANDES; ++b) {
        Model          m;
        const Texture* tex = m.acquireTexture(dir / "Interior/voyants.png");
        if (tex == nullptr) {
            m_voyants.clear();
            break;
        }
        ajouterCoupole(m, tex, b);
        m_voyants.push_back(std::move(m));
    }
    /* Rose du compas : seulement si sa bande est là (sinon le quad prendrait
       la dernière texture liée). */
    Model          rose;
    const Texture* bande = rose.acquireTexture(dir / "Interior/rose-compas.png");
    if (bande != nullptr) {
        m_roseCompas = roseCompas(bande, std::move(rose));
    }
}

void LoadedHelicopter::dessinerPlancheGltf(Shader& shader, const mat4& root) const {
    const EtatPlanche& e    = m_etatPlanche;
    int                rang = 0;
    /* Une aiguille sur le cadran 'c' : pivot décalé de 'pivot' rayons vers
       midi (négatif : vers le bas), longueur et largeur en rayons, angle
       depuis midi dans le sens horaire vu du pilote. */
    const auto aiguille = [&](const CadranDessine& c, float pivot, float longueur, float largeur,
                              float angleDeg) {
        const float devant = ECART_AIGUILLES * static_cast<float>(rang++);
        const mat4  m = root * repere(c) *
                       glm::translate(mat4(1.0f), vec3{devant, pivot * c.rayon, 0.0f}) *
                       glm::rotate(mat4(1.0f), glm::radians(angleDeg), vec3{-1.0f, 0.0f, 0.0f}) *
                       glm::scale(mat4(1.0f), vec3{1.0f, longueur * c.rayon, largeur * c.rayon});
        drawModel(shader, m_aiguille, m, Pass::Opaque);
    };

    /* Tachymètre double : rotor (courte) sous turbine (longue). */
    aiguille(TACHYMETRE, 0.0f, 0.55f, 0.10f, angleSur(ECHELLE_ROTOR, e.rotorTpm));
    aiguille(TACHYMETRE, 0.0f, 0.85f, 0.07f, angleSur(ECHELLE_TURBINE, e.turbineTpm));
    /* Unité triple : tuyère au centre, huile autour du pivot bas. */
    aiguille(UNITE_TRIPLE, 0.0f, 0.80f, 0.07f, angleSur(ECHELLE_TUYERE, e.tuyereC));
    aiguille(UNITE_TRIPLE, -0.45f, 0.42f, 0.07f, angleSur(ECHELLE_HUILE_C, e.huileC));
    aiguille(UNITE_TRIPLE, -0.45f, 0.42f, 0.07f, angleSur(ECHELLE_HUILE_BAR, e.huileBar));
    aiguille(PAS_COLLECTIF, 0.0f, 0.85f, 0.07f, angleSur(ECHELLE_PAS, e.pasDeg));
    aiguille(VOLTMETRE, 0.0f, 0.80f, 0.08f, angleSur(ECHELLE_VOLTS, e.volts));
    aiguille(CARBURANT, 0.0f, 0.80f, 0.08f, angleSur(ECHELLE_CARBURANT, gallonsUs(e.carburantL)));
    aiguille(SERVO, 0.0f, 0.80f, 0.09f, angleSur(ECHELLE_SERVO, e.servoBar));
    /* Côté copilote : indicateur de virage (aiguille et bille) et
       thermomètre extérieur. La bille se décale à droite quand elle est
       poussée à droite (droite du pilote vers -Z). */
    aiguille(VIRAGE, 0.0f, 0.80f, 0.09f, angleSur(ECHELLE_VIRAGE, e.virageDegS));
    aiguille(AIR, 0.0f, 0.80f, 0.09f, angleSur(ECHELLE_AIR, e.airC));
    {
        const float t = glm::radians(
            glm::clamp(e.billeG * BILLE_DEG_PAR_G, -BILLE_BUTEE_DEG, BILLE_BUTEE_DEG));
        const float R = VIRAGE.rayon;
        const vec3  ou{ECART_AIGUILLES * static_cast<float>(rang++),
                       (BILLE_CENTRE - BILLE_ARC * std::cos(t)) * R,
                       -BILLE_ARC * std::sin(t) * R};
        drawModel(shader, m_bille,
                  root * repere(VIRAGE) * glm::translate(mat4(1.0f), ou) *
                      glm::scale(mat4(1.0f), vec3{1.0f, BILLE_RAYON * R, BILLE_RAYON * R}),
                  Pass::Opaque);
    }

    /* Montre : l'heure du jeu. */
    const float h = std::fmod(e.heure, 12.0f);
    aiguille(MONTRE, 0.0f, 0.55f, 0.12f, h * 30.0f);
    aiguille(MONTRE, 0.0f, 0.85f, 0.08f, std::fmod(h, 1.0f) * 360.0f);

    /* Voyants : coupole de verre coloré, sombre éteinte ; allumée, elle luit
       de sa couleur en gardant un peu d'ombrage pour son relief. */
    if (static_cast<int>(m_voyants.size()) == NB_BANDES) {
        const auto voyant = [&](bool allume, int allumee, int eteinte, float z) {
            const vec3 centre{VOYANT_X, VOYANT_Y, z};
            shader.setFloat("u_emissif", allume ? 0.85f : 0.0f);
            drawModel(shader, m_voyants[static_cast<std::size_t>(allume ? allumee : eteinte)],
                      root * placementCadran(centre, VOYANTS_NORMALE, VOYANT_RAYON,
                                             VOYANT_DEVANT),
                      Pass::Opaque);
        };
        voyant(e.voyantRelais, ROUGE_ALLUME, ROUGE_ETEINT, VOYANT_RELAIS_Z);
        voyant(e.voyantDemarreur, VERT_ALLUME, VERT_ETEINT, VOYANT_DEMARREUR_Z);
        voyant(e.voyantMicropompe, AMBRE_ALLUME, AMBRE_ETEINT, VOYANT_MICROPOMPE_Z);
        voyant(e.voyantGeneratrice, ROUGE_ALLUME, ROUGE_ETEINT, VOYANT_GENERATRICE_Z);
        voyant(e.voyantHuileBtp, ROUGE_ALLUME, ROUGE_ETEINT, VOYANT_HUILE_BTP_Z);
        shader.setFloat("u_emissif", 0.0f);
    }

    /* Compas magnétique : la rose défile (le nord au bord gauche de la bande,
       les caps croissant vers la gauche) pour mettre le cap au centre, sous
       la ligne de foi. */
    if (!m_roseCompas.empty()) {
        const mat4 fenetre = root * placementCadran(COMPAS_CENTRE, COMPAS_NORMALE, 1.0f,
                                                    DEVANT_FACE);
        shader.setVec2("u_uvDecalage", vec2{1.0f - e.capDeg / 360.0f, 0.0f});
        drawModel(shader, m_roseCompas, fenetre, Pass::Opaque);
        shader.setVec2("u_uvDecalage", vec2{0.0f});
        const mat4 foi = fenetre *
                         glm::translate(mat4(1.0f), vec3{0.0005f, -0.5f * COMPAS_HAUTEUR, 0.0f}) *
                         glm::scale(mat4(1.0f), vec3{1.0f, COMPAS_HAUTEUR, 0.0012f});
        drawModel(shader, m_aiguille, foi, Pass::Opaque);
    }
}

/* Le shader reconnaît les faces à leur position dans le repère du maillage de
   la cellule (quart de tour m_fuselageFix défait). Rayon : un peu plus que la
   face (décalage compris), moins que la lunette (face à 90 % du disque). La
   taille du tableau est celle de u_faces dans model.frag. */
void LoadedHelicopter::lampesFacesGltf(Shader& shader, bool allumees) const {
    if (!allumees) {
        shader.setInt("u_nbFaces", 0);
        return;
    }
    static const std::array<const CadranDessine*, 9> FACES{
        &TACHYMETRE, &UNITE_TRIPLE, &PAS_COLLECTIF, &VOLTMETRE, &CARBURANT,
        &SERVO,      &MONTRE,       &VIRAGE,        &AIR};
    const mat4                  versMaillage = glm::inverse(m_fuselageFix);
    std::array<vec4, FACES.size()> faces{};
    for (std::size_t i = 0; i < FACES.size(); ++i) {
        faces[i] = vec4(vec3(versMaillage * repere(*FACES[i])[3]), FACES[i]->rayon * 1.05f);
    }
    shader.setVec4Array("u_faces", &faces[0].x, static_cast<int>(faces.size()));
    shader.setInt("u_nbFaces", static_cast<int>(faces.size()));
}

}  /* namespace artouste::render */
