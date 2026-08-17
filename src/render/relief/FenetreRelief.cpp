/*
 * FenetreRelief.cpp
 * Implémentation de la fenêtre de relief fin : lecture de l'index et des tuiles,
 * texture torique d'altitudes, grille dessinée et interrogation du sol (voir
 * FenetreRelief.hpp).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "render/relief/FenetreRelief.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace artouste::render::relief {

namespace {

/* En-tête d'une tuile, tel que tools/terrain/fetch_relief.py l'écrit : 20 octets
   petit-boutiens, puis côté x côté entiers 16 bits. */
constexpr char          MAGIQUE[4]     = {'A', 'R', 'T', 'R'};
constexpr std::size_t   EN_TETE_OCTETS = 20;
constexpr std::uint16_t VERSION        = 1;

/* Reprise du ruban de triangles. Un ruban par rangée : 8 Mo d'indices au lieu de
   25 pour une grille de 1024. */
constexpr unsigned int REPRISE = 0xFFFFFFFFu;

/* Reste toujours positif : un point à l'ouest de l'ancre a un indice négatif. */
[[nodiscard]] int modulo(int a, int b) noexcept {
    const int r = a % b;
    return (r < 0) ? r + b : r;
}

/* Lit un entier 16 bits petit-boutien. */
[[nodiscard]] std::uint16_t lire16(const unsigned char* p) noexcept {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8);
}

/* Lit un flottant 32 bits petit-boutien. */
[[nodiscard]] float lireFlottant(const unsigned char* p) noexcept {
    float valeur = 0.0f;
    std::memcpy(&valeur, p, sizeof(valeur));
    return valeur;
}

}  /* namespace */

std::unique_ptr<FenetreRelief> FenetreRelief::ouvrir(const std::filesystem::path& dossier,
                                                     Correcteur                   correcteur,
                                                     int coteGrillePoints) {
    std::ifstream in(dossier / NOM_INDEX);
    if (!in) {
        return nullptr;
    }

    /* Même convention que l'index des tuiles d'image : une clé par ligne, # en
       commentaire, clé inconnue ignorée. */
    Calage      calage;
    std::string cle;
    while (in >> cle) {
        if (!cle.empty() && cle[0] == '#') {
            std::getline(in, cle);
            continue;
        }
        if (cle == "tuile_points") {
            in >> calage.tuilePoints;
        } else if (cle == "pas_m") {
            in >> calage.pasM;
        } else if (cle == "colonnes") {
            in >> calage.colonnes;
        } else if (cle == "rangees") {
            in >> calage.rangees;
        } else if (cle == "coin_x") {
            in >> calage.coinX;
        } else if (cle == "coin_z") {
            in >> calage.coinZ;
        } else {
            std::getline(in, cle);
        }
    }

    if (!calage.valide() || !correcteur) {
        return nullptr;
    }

    std::unique_ptr<FenetreRelief> fenetre(
        new FenetreRelief(dossier, calage, std::move(correcteur), coteGrillePoints));
    if (!fenetre->allouerTexture()) {
        return nullptr;
    }

    /* Noyau au pas des tuiles, puis anneau quatre fois plus grossier et quatre
       fois plus large. L'anneau est écarté s'il déborde des tuiles que le tore
       garantit présentes. */
    const float marge = static_cast<float>(TUILES_FENETRE / 2 - 1) * calage.tuileM();
    const int   noyau = std::clamp(coteGrillePoints, 64, fenetre->m_cotePoints / 2);
    if (!fenetre->construireGrille(noyau, calage.pasM)) {
        return nullptr;
    }
    const float pasAnneau  = static_cast<float>(PAS_ANNEAU) * calage.pasM;
    const float demiAnneau = 0.5f * static_cast<float>(COTE_ANNEAU_POINTS - 1) * pasAnneau;
    if (demiAnneau <= marge) {
        (void)fenetre->construireGrille(COTE_ANNEAU_POINTS, pasAnneau);
    }

    for (const Grille& grille : fenetre->m_grilles) {
        std::printf("[relief] grille %d points à %.1f m : %.0f m au sol, %.2f M sommets.\n",
                    grille.cote,
                    static_cast<double>(grille.pas),
                    static_cast<double>(grille.cote - 1) * static_cast<double>(grille.pas),
                    static_cast<double>(grille.cote) * static_cast<double>(grille.cote) / 1e6);
    }
    {
        const Grille& noyau     = fenetre->m_grilles.front();
        const Grille& large     = fenetre->m_grilles.back();
        const float   demiNoyau = 0.5f * static_cast<float>(noyau.cote - 1) * noyau.pas;
        const float   rapport   = std::max(1.0f, large.pas / calage.pasM);
        std::printf("[relief] finesse pleine jusqu'à %.1f m, ÉPINGLÉE (loi 2x : %.1f m).\n",
                    static_cast<double>(fenetre->distanceDetailM()),
                    static_cast<double>((demiNoyau - 0.5f * large.pas) / (2.0f * rapport)));
    }
    std::printf("[relief] fenêtre %d x %d points à %.2f m, %.0f m au sol, %.0f Mo.\n",
                fenetre->m_cotePoints,
                fenetre->m_cotePoints,
                static_cast<double>(calage.pasM),
                static_cast<double>(fenetre->tailleM()),
                static_cast<double>(fenetre->m_hauteurs.size()) * sizeof(float) / 1e6);
    return fenetre;
}

FenetreRelief::FenetreRelief(std::filesystem::path dossier, const Calage& calage,
                             Correcteur correcteur, int coteGrillePoints)
    : m_dossier(std::move(dossier)), m_calage(calage), m_correcteur(std::move(correcteur)) {
    (void)coteGrillePoints;  /* les grilles sont construites par ouvrir() */
    m_cotePoints = TUILES_FENETRE * m_calage.tuilePoints;
    m_emplacements.assign(static_cast<std::size_t>(TUILES_FENETRE) * TUILES_FENETRE,
                          Emplacement{});
    m_hauteurs.assign(static_cast<std::size_t>(m_cotePoints) *
                          static_cast<std::size_t>(m_cotePoints),
                      0.0f);
}

FenetreRelief::~FenetreRelief() {
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
    }
    for (Grille& grille : m_grilles) {
        if (grille.ebo != 0) {
            glDeleteBuffers(1, &grille.ebo);
        }
        if (grille.vao != 0) {
            glDeleteVertexArrays(1, &grille.vao);
        }
    }
}

bool FenetreRelief::allouerTexture() {
    /* Pile d'erreurs vidée : une erreur antérieure nous ferait renoncer à tort. */
    while (glGetError() != GL_NO_ERROR) {
    }

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_cotePoints, m_cotePoints, 0, GL_RED, GL_FLOAT,
                 m_hauteurs.data());
    /* La répétition fait le tore. Les niveaux de réduction servent au lissage à
       la maille de la carte, pas au filtrage de distance : le shader les demande
       explicitement (voir niveauLissage). */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (glGetError() != GL_NO_ERROR) {
        std::fprintf(stderr, "[relief] texture %d x %d refusée : pas de fenêtre de relief.\n",
                     m_cotePoints, m_cotePoints);
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
        return false;
    }
    return true;
}

bool FenetreRelief::construireGrille(int cote, float pas) {
    /* Aucun sommet stocké : terrain.vert les calcule depuis gl_VertexID. Seuls
       les indices existent, en rubans séparés par un indice de reprise. */
    std::vector<unsigned int> indices;
    indices.reserve(static_cast<std::size_t>(cote - 1) * (2 * static_cast<std::size_t>(cote) + 1));
    for (int j = 0; j < cote - 1; ++j) {
        for (int i = 0; i < cote; ++i) {
            indices.push_back(static_cast<unsigned int>(j * cote + i));
            indices.push_back(static_cast<unsigned int>((j + 1) * cote + i));
        }
        indices.push_back(REPRISE);
    }

    Grille grille;
    grille.cote    = cote;
    grille.pas     = pas;
    grille.indices = static_cast<int>(indices.size());
    glGenVertexArrays(1, &grille.vao);
    glBindVertexArray(grille.vao);
    glGenBuffers(1, &grille.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grille.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(),
                 GL_STATIC_DRAW);
    glBindVertexArray(0);
    if (grille.vao == 0 || grille.ebo == 0) {
        return false;
    }
    m_grilles.push_back(grille);
    return true;
}

bool FenetreRelief::lireTuile(int col, int rangee, std::vector<float>& hauteurs) const {
    const std::filesystem::path chemin =
        m_dossier / std::to_string(rangee) / (std::to_string(col) + ".r16");
    std::ifstream in(chemin, std::ios::binary);
    if (!in) {
        return false;
    }

    const int         cote   = m_calage.tuilePoints;
    const std::size_t points = static_cast<std::size_t>(cote) * static_cast<std::size_t>(cote);
    const std::size_t attendu = EN_TETE_OCTETS + points * 2;
    std::vector<unsigned char> brut(attendu);
    in.read(reinterpret_cast<char*>(brut.data()), static_cast<std::streamsize>(attendu));
    if (static_cast<std::size_t>(in.gcount()) != attendu) {
        return false;
    }
    if (std::memcmp(brut.data(), MAGIQUE, sizeof(MAGIQUE)) != 0 ||
        lire16(brut.data() + 4) != VERSION ||
        lire16(brut.data() + 6) != static_cast<std::uint16_t>(cote)) {
        return false;
    }

    const float mini    = lireFlottant(brut.data() + 12);
    const float etendue = lireFlottant(brut.data() + 16);
    const float echelle = etendue / 65535.0f;
    const unsigned char* niveaux = brut.data() + EN_TETE_OCTETS;
    for (std::size_t k = 0; k < points; ++k) {
        hauteurs[k] = mini + static_cast<float>(lire16(niveaux + 2 * k)) * echelle;
    }
    return true;
}

void FenetreRelief::poserTuile(int col, int rangee) {
    const int         cote   = m_calage.tuilePoints;
    const std::size_t largeur = static_cast<std::size_t>(cote);
    std::vector<float> hauteurs(largeur * largeur, 0.0f);

    /* Tuile absente : hors couverture LiDAR, la fabrication ne l'écrit pas.
       L'emplacement reçoit alors le relief d'ensemble. */
    const bool aDonnee = lireTuile(col, rangee, hauteurs);

    const float x0 = m_calage.coinX + static_cast<float>(col) * m_calage.tuileM();
    const float z0 = m_calage.coinZ + static_cast<float>(rangee) * m_calage.tuileM();
    m_correcteur(x0, z0, m_calage.pasM, cote, hauteurs.data(), aDonnee);

    const int slotCol    = modulo(col, TUILES_FENETRE);
    const int slotRangee = modulo(rangee, TUILES_FENETRE);

    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, slotCol * cote, slotRangee * cote, cote, cote, GL_RED,
                    GL_FLOAT, hauteurs.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Copie CPU, même disposition torique : c'est elle que lit hauteurEn. */
    for (int j = 0; j < cote; ++j) {
        const std::size_t dst = static_cast<std::size_t>(slotRangee * cote + j) *
                                    static_cast<std::size_t>(m_cotePoints) +
                                static_cast<std::size_t>(slotCol * cote);
        std::memcpy(m_hauteurs.data() + dst, hauteurs.data() + static_cast<std::size_t>(j) * largeur,
                    largeur * sizeof(float));
    }

    m_emplacements[static_cast<std::size_t>(slotRangee * TUILES_FENETRE + slotCol)] =
        Emplacement{col, rangee};
}

void FenetreRelief::suivre(float x, float z) {
    if (m_texture == 0) {
        return;
    }

    /* Centre calé sur la période commune des réseaux (voir CALAGE_MAILLES).
       Calé plus fin, un réseau glisse sur l'autre à chaque pas : les triangles
       recordent le champ ailleurs et les normales sautent. */
    const float pas = m_calage.pasM * static_cast<float>(CALAGE_MAILLES);
    m_centreX = m_calage.coinX + std::round((x - m_calage.coinX) / pas) * pas;
    m_centreZ = m_calage.coinZ + std::round((z - m_calage.coinZ) / pas) * pas;

    /* Le centre calé pose les sommets, et rien d'autre. Le fondu et la finesse
       se mesurent depuis l'oeil, resté continu. */
    m_oeilX = x;
    m_oeilZ = z;

    /* Tuiles tenues : celle de la caméra, autant que possible de part et
       d'autre. La marge garantie sur chaque bord vaut alors
       (TUILES_FENETRE / 2 - 1) tuiles, où que la caméra soit dans la sienne. */
    const float tuile = m_calage.tuileM();
    const int   colCam    = static_cast<int>(std::floor((m_oeilX - m_calage.coinX) / tuile));
    const int   rangeeCam = static_cast<int>(std::floor((m_oeilZ - m_calage.coinZ) / tuile));

    /* Première image : tout d'un coup, sinon le sol s'affaisserait le temps du
       remplissage. Ensuite, quelques tuiles par image suffisent, l'appareil
       mettant des secondes à parcourir la marge. */
    const int budget = m_premier ? TUILES_FENETRE * TUILES_FENETRE : TUILES_PAR_IMAGE;

    const int premier = -(TUILES_FENETRE / 2 - 1);
    const int dernier = TUILES_FENETRE / 2;

    int posees = 0;
    for (int dr = premier; dr <= dernier && posees < budget; ++dr) {
        for (int dc = premier; dc <= dernier && posees < budget; ++dc) {
            const int col    = colCam + dc;
            const int rangee = rangeeCam + dr;
            const Emplacement& place =
                m_emplacements[static_cast<std::size_t>(modulo(rangee, TUILES_FENETRE)) *
                                   TUILES_FENETRE +
                               static_cast<std::size_t>(modulo(col, TUILES_FENETRE))];
            if (place.col == col && place.rangee == rangee) {
                continue;
            }
            poserTuile(col, rangee);
            ++posees;
        }
    }
    if (posees > 0) {
        /* Les niveaux de réduction portent le lissage à la maille de la carte :
           ils doivent suivre les tuiles qui viennent d'arriver. Une seule
           régénération pour toutes, elle parcourt la texture entière. */
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    m_premier = false;
}

void FenetreRelief::bind(unsigned int unite) const {
    glActiveTexture(GL_TEXTURE0 + unite);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glActiveTexture(GL_TEXTURE0);
}

void FenetreRelief::dessiner(int niveau) const {
    const Grille& grille = m_grilles[static_cast<std::size_t>(niveau)];
    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(REPRISE);
    glBindVertexArray(grille.vao);
    glDrawElements(GL_TRIANGLE_STRIP, grille.indices, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glDisable(GL_PRIMITIVE_RESTART);
}

float FenetreRelief::distanceDetailM() const noexcept {
    const Grille& noyau = m_grilles.front();
    const Grille& large = m_grilles.back();
    const float   demiNoyau = 0.5f * static_cast<float>(noyau.cote - 1) * noyau.pas;
    const float   rapport   = std::max(1.0f, large.pas / m_calage.pasM);
    /* ÉPINGLÉ à la valeur d'avant le report, le temps de la recette en vol :
       Olivier juge deux composantes de lumière, pas une portée de détail réduite
       de moitié qu'il n'a jamais vue.

       L'autre loi, (demiNoyau - demi-pas de calage) / (2 x rapport), fait lire à
       l'anneau un champ deux fois plus grossier que son pas, ce qui efface la
       marche au bord du noyau, mais divise cette distance par deux. Elle se
       tranchera avec le choix C/B au report. Le journal imprime les deux. */
    return demiNoyau / rapport;
}

float FenetreRelief::niveauLissage() const noexcept {
    /* Un niveau de réduction moyenne deux points par deux : le niveau n lisse
       sur 2^n points. On prend le plus proche de la maille de la carte. */
    return std::max(0.0f, std::round(std::log2(MAILLE_CARTE_M / m_calage.pasM)));
}

bool FenetreRelief::detailEn(float x, float z, float& detail, float& poids) const noexcept {
    /* Tant qu'aucune tuile n'est posée, les altitudes sont nulles : répondre
       ferait naître l'appareil mille mètres sous le sol, le placement du départ
       interrogeant le terrain avant la première image. */
    if (m_texture == 0 || m_premier) {
        return false;
    }

    /* Fondu du bord, MÊME formule que terrain.vert : distance à l'oeil, adoucie
       comme son smoothstep. */
    const float bord  = std::sqrt((x - m_oeilX) * (x - m_oeilX) +
                                 (z - m_oeilZ) * (z - m_oeilZ));
    const float debut = fonduDebutM();
    const float fin   = fonduFinM();
    if (bord >= fin) {
        return false;
    }
    poids = (bord <= debut) ? 1.0f : 1.0f - (bord - debut) / (fin - debut);
    poids = poids * poids * (3.0f - 2.0f * poids);

    const float pas = m_calage.pasM;
    const float fi  = (x - m_calage.coinX) / pas;
    const float fj  = (z - m_calage.coinZ) / pas;
    const int   i0  = static_cast<int>(std::floor(fi));
    const int   j0  = static_cast<int>(std::floor(fj));
    const float tx  = fi - static_cast<float>(i0);
    const float tz  = fj - static_cast<float>(j0);

    const auto at = [this](int i, int j) {
        return m_hauteurs[static_cast<std::size_t>(modulo(j, m_cotePoints)) *
                              static_cast<std::size_t>(m_cotePoints) +
                          static_cast<std::size_t>(modulo(i, m_cotePoints))];
    };
    /* Le point lu à un niveau de réduction donné, exactement comme la carte
       graphique le calcule : moyenne de blocs de 2^niveau, puis interpolation
       bilinéaire entre les blocs. */
    const auto niveau = [&](int n) {
        const int   cote = 1 << n;
        const float gi   = (fi + 0.5f) / static_cast<float>(cote) - 0.5f;
        const float gj   = (fj + 0.5f) / static_cast<float>(cote) - 0.5f;
        const int   mi   = static_cast<int>(std::floor(gi));
        const int   mj   = static_cast<int>(std::floor(gj));
        const float ux   = gi - static_cast<float>(mi);
        const float uz   = gj - static_cast<float>(mj);

        const auto bloc = [&at, cote](int bi, int bj) {
            float somme = 0.0f;
            for (int j = 0; j < cote; ++j) {
                for (int i = 0; i < cote; ++i) {
                    somme += at(bi * cote + i, bj * cote + j);
                }
            }
            return somme / static_cast<float>(cote * cote);
        };
        const float a = bloc(mi, mj);
        const float b = bloc(mi + 1, mj);
        const float c = bloc(mi, mj + 1);
        const float d = bloc(mi + 1, mj + 1);
        return (a * (1.0f - ux) + b * ux) * (1.0f - uz) + (c * (1.0f - ux) + d * ux) * uz;
    };

    /* Finesse selon la distance, MÊME formule que terrain.vert (finesseDetail) :
       pleine finesse jusqu'à distanceDetailM, puis résolution divisée par deux à
       chaque doublement de distance. */
    const float lissage = niveauLissage();
    const float finesse =
        std::clamp(std::log2(std::max(bord, 1.0f) / distanceDetailM()), 0.0f, lissage);

    /* Le niveau 0 est le cas courant, l'appareil volant près du centre : on lui
       garde le chemin direct, une simple bilinéaire, plutôt que la moyenne de
       blocs qui lui serait équivalente. */
    const int   bas = static_cast<int>(std::floor(finesse));
    const float f   = finesse - static_cast<float>(bas);
    const auto  at0 = [&](int n) {
        return (n == 0) ? (at(i0, j0) * (1.0f - tx) + at(i0 + 1, j0) * tx) * (1.0f - tz) +
                              (at(i0, j0 + 1) * (1.0f - tx) + at(i0 + 1, j0 + 1) * tx) * tz
                        : niveau(n);
    };
    const float lu = (f <= 0.0f) ? at0(bas) : at0(bas) * (1.0f - f) + at0(bas + 1) * f;

    detail = lu - niveau(static_cast<int>(lissage));
    return true;
}

std::filesystem::path cheminJeuDeRelief(const std::filesystem::path& dossierCarte,
                                        const std::filesystem::path& racine) {
    const std::string nom = dossierCarte.filename().string() + ".relief";

    std::vector<std::filesystem::path> candidats;
    if (const char* env = std::getenv("ARTOUSTE_TUILES"); env != nullptr && env[0] != '\0') {
        candidats.push_back(std::filesystem::path(env) / nom);
    }
    if (!racine.empty()) {
        candidats.push_back(racine / nom);
    }
    candidats.push_back(dossierCarte.parent_path() / nom);
    candidats.push_back(dossierCarte / "relief");

    std::error_code ec;
    for (const std::filesystem::path& candidat : candidats) {
        if (std::filesystem::exists(candidat / NOM_INDEX, ec)) {
            return candidat;
        }
    }
    return {};
}

}  /* namespace artouste::render::relief */
