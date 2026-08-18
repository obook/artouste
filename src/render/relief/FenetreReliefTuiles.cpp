/*
 * FenetreReliefTuiles.cpp
 * Texture torique, grilles de dessin, lecture et pose des tuiles.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/relief/FenetreRelief.hpp"

#include "render/relief/FenetreReliefInterne.hpp"

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

bool FenetreRelief::construireGrille(int cote, float pasX, float pasZ) {
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
    grille.pasX    = pasX;
    grille.pasZ    = pasZ;
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
    /* Deux versions coexistent : v1 porte un pas unique, v2 un pas par axe, et
       son en-tête compte donc quatre octets de plus. Une carte migrée cohabite
       ainsi avec les autres restées en v1. */
    std::vector<unsigned char> brut(EN_TETE_V2_OCTETS + points * 2);
    in.read(reinterpret_cast<char*>(brut.data()), static_cast<std::streamsize>(brut.size()));
    const std::size_t lus = static_cast<std::size_t>(in.gcount());
    if (std::memcmp(brut.data(), MAGIQUE, sizeof(MAGIQUE)) != 0 ||
        lire16(brut.data() + 6) != static_cast<std::uint16_t>(cote)) {
        return false;
    }
    const std::uint16_t version = lire16(brut.data() + 4);
    const std::size_t   enTete  = (version == 2) ? EN_TETE_V2_OCTETS : EN_TETE_OCTETS;
    if ((version != 1 && version != 2) || lus != enTete + points * 2) {
        return false;
    }

    /* Le pas suit l'en-tête : un flottant en v1, deux en v2. */
    const std::size_t decalage = enTete - 8;
    const float mini    = lireFlottant(brut.data() + decalage);
    const float etendue = lireFlottant(brut.data() + decalage + 4);
    const float echelle = etendue / 65535.0f;
    const unsigned char* niveaux = brut.data() + enTete;
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

    const float x0 = m_calage.coinX + static_cast<float>(col) * m_calage.tuileX();
    const float z0 = m_calage.coinZ + static_cast<float>(rangee) * m_calage.tuileZ();
    m_correcteur(x0, z0, m_calage.pasX, m_calage.pasZ, cote, hauteurs.data(), aDonnee);

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
    const float pasX = m_calage.pasX * static_cast<float>(CALAGE_MAILLES);
    const float pasZ = m_calage.pasZ * static_cast<float>(CALAGE_MAILLES);
    m_centreX = m_calage.coinX + std::round((x - m_calage.coinX) / pasX) * pasX;
    m_centreZ = m_calage.coinZ + std::round((z - m_calage.coinZ) / pasZ) * pasZ;

    /* Le centre calé pose les sommets, et rien d'autre. Le fondu et la finesse
       se mesurent depuis l'oeil, resté continu. */
    m_oeilX = x;
    m_oeilZ = z;

    /* Tuiles tenues : celle de la caméra, autant que possible de part et
       d'autre. La marge garantie sur chaque bord vaut alors
       (TUILES_FENETRE / 2 - 1) tuiles, où que la caméra soit dans la sienne. */
    const int colCam =
        static_cast<int>(std::floor((m_oeilX - m_calage.coinX) / m_calage.tuileX()));
    const int rangeeCam =
        static_cast<int>(std::floor((m_oeilZ - m_calage.coinZ) / m_calage.tuileZ()));

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

} /* namespace artouste::render::relief */
