/*
 * Fenetre.cpp
 * Implémentation de la fenêtre de détail : allocation de la texture torique,
 * suivi de l'appareil, lecture des tuiles en fil de fond et envoi au GPU (voir
 * Fenetre.hpp).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "render/tuiles/Fenetre.hpp"

#include "render/Texture.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace artouste::render::tuiles {

namespace {

/* Même jeton qu'ailleurs dans le moteur : BC7 est cœur depuis OpenGL 4.2 et le
   profil glad du projet est figé en 4.1 (voir Texture.cpp). */
#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM
constexpr unsigned int GL_COMPRESSED_RGBA_BPTC_UNORM = 0x8E8C;
#endif
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
constexpr unsigned int GL_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FE;
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
constexpr unsigned int GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF;
#endif

/* Nombre de tuiles qu'on accepte de garder lues d'avance. Une tuile pèse
   341 Ko : au-delà d'une poignée, le fil de fond travaillerait pour des
   emplacements que la fenêtre a déjà quittés. */
constexpr std::size_t PRETES_MAX = 12;

/* Reste toujours positif, pour indexer un emplacement du tore : une tuile à
   l'ouest ou au nord de l'ancre a un indice négatif, dont le reste du C++ est
   négatif lui aussi. */
[[nodiscard]] int modulo(int a, int b) noexcept {
    const int r = a % b;
    return (r < 0) ? r + b : r;
}

}  /* namespace */

Fenetre::Fenetre(Pyramide pyramide, int cotePx) : m_pyramide(std::move(pyramide)) {
    const Calage& calage = m_pyramide.calage();
    if (!calage.valide()) {
        return;
    }
    if (!bc7Disponible()) {
        std::fprintf(stderr, "[tuiles] BC7 indisponible : pas de fenêtre de détail.\n");
        return;
    }

    /* La fenêtre est un nombre entier de tuiles : on rabote plutôt que
       d'accepter un emplacement tronqué, dont les envois partiels ne
       tomberaient plus sur des frontières de blocs BC7. */
    m_nbTuiles = std::max(2, cotePx / calage.tuilePx);
    m_cotePx   = m_nbTuiles * calage.tuilePx;

    m_emplacements.assign(static_cast<std::size_t>(m_nbTuiles) *
                              static_cast<std::size_t>(m_nbTuiles),
                          Emplacement{});
    m_masque.assign(m_emplacements.size(), 0);

    allouerTexture();
    if (m_texture == 0) {
        return;
    }
    allouerResidence();

    m_fil = std::thread(&Fenetre::boucleLecture, this);

    std::printf("[tuiles] fenêtre %d px (%d x %d tuiles de %d px), %.3f m/px, %.0f m au sol.\n",
                m_cotePx,
                m_nbTuiles,
                m_nbTuiles,
                calage.tuilePx,
                static_cast<double>(calage.mParPixel),
                static_cast<double>(tailleM()));
}

Fenetre::~Fenetre() {
    m_fini = true;
    m_signal.notify_all();
    if (m_fil.joinable()) {
        m_fil.join();
    }
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
    }
    if (m_residence != 0) {
        glDeleteTextures(1, &m_residence);
    }
}

void Fenetre::allouerTexture() {
    /* OpenGL 4.1 n'a pas glTexStorage2D : on réserve chaque niveau par un envoi
       de blocs vides. Un bloc BC7 nul ne décode rien de bon, mais aucun
       fragment ne le lira jamais : la texture de résidence tient un emplacement
       à zéro tant que sa tuile n'est pas arrivée. */
    const int          tuilePx = m_pyramide.calage().tuilePx;
    const std::size_t  octets0 = dds::octetsBc7(m_cotePx, m_cotePx);
    std::vector<unsigned char> vides(octets0, 0);

    /* On part d'une pile d'erreurs vide, sinon une erreur laissée par un appel
       antérieur nous ferait renoncer à tort à la fenêtre. */
    while (glGetError() != GL_NO_ERROR) {
    }

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    int niveaux = 0;
    for (int i = 0; i < NIVEAUX_FENETRE; ++i) {
        const int cote = m_cotePx >> i;
        /* On s'arrête si un emplacement descendrait sous un bloc BC7 : ses
           envois partiels ne seraient plus alignés. */
        if ((tuilePx >> i) < 4) {
            break;
        }
        glCompressedTexImage2D(GL_TEXTURE_2D,
                               i,
                               GL_COMPRESSED_RGBA_BPTC_UNORM,
                               cote,
                               cote,
                               0,
                               static_cast<GLsizei>(dds::octetsBc7(cote, cote)),
                               vides.data());
        ++niveaux;
    }
    if (glGetError() != GL_NO_ERROR) {
        std::fprintf(stderr,
                     "[tuiles] mémoire vidéo refusée pour une fenêtre de %d px : "
                     "pas de fenêtre de détail.\n",
                     m_cotePx);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
        return;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, niveaux - 1);
    /* Répétition : c'est elle qui referme le tore. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /* Filtrage anisotrope, comme pour l'orthophoto d'ensemble : sans lui, le sol
       vu en rasant est sur-réduit et redevient pâté, ce qui annulerait tout le
       bénéfice du détail là où il compte le plus, juste devant l'appareil (voir
       Texture::reglerFiltrage). */
    GLfloat maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(maxAniso, 16.0f));

    glBindTexture(GL_TEXTURE_2D, 0);
}

void Fenetre::allouerResidence() {
    /* Un octet par emplacement, lu au plus proche : le shader veut l'état de
       SON emplacement, pas une moyenne avec les voisins, qui étalerait le
       détail sur une tuile absente. */
    glGenTextures(1, &m_residence);
    glBindTexture(GL_TEXTURE_2D, m_residence);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_R8,
                 m_nbTuiles,
                 m_nbTuiles,
                 0,
                 GL_RED,
                 GL_UNSIGNED_BYTE,
                 m_masque.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

Fenetre::Emplacement& Fenetre::emplacement(int col, int rangee) noexcept {
    const int i = modulo(rangee, m_nbTuiles) * m_nbTuiles + modulo(col, m_nbTuiles);
    return m_emplacements[static_cast<std::size_t>(i)];
}

const Fenetre::Emplacement& Fenetre::emplacement(int col, int rangee) const noexcept {
    const int i = modulo(rangee, m_nbTuiles) * m_nbTuiles + modulo(col, m_nbTuiles);
    return m_emplacements[static_cast<std::size_t>(i)];
}

float Fenetre::rayonPleinM() const noexcept {
    return 0.62f * rayonFonduM();
}

float Fenetre::rayonFonduM() const noexcept {
    /* La fenêtre est centrée sur la TUILE de l'appareil, pas sur l'appareil :
       la marge garantie de chaque côté est donc d'un demi-côté moins une tuile.
       Au-delà, la présence de la tuile n'est pas assurée et le détail doit déjà
       avoir laissé la place à l'orthophoto d'ensemble. */
    const float tuileM = m_pyramide.calage().tuileM();
    return (static_cast<float>(m_nbTuiles) / 2.0f - 1.0f) * tuileM;
}

void Fenetre::suivre(float x, float z, float dt) {
    if (m_texture == 0) {
        return;
    }
    const Calage& calage = m_pyramide.calage();
    const float   tuileM = calage.tuileM();

    /* Tuile de l'appareil, même si elle tombe hors de la grille : la fenêtre
       reste centrée sur lui, les emplacements hors grille restent simplement
       vides. Le plancher est pris avant la conversion, sans quoi la troncature
       du C++ ramènerait une tuile d'indice négatif (à l'ouest ou au nord de
       l'ancre) sur la tuile 0. */
    const int centreCol    = static_cast<int>(std::floor((x - calage.coinX) / tuileM));
    const int centreRangee = static_cast<int>(std::floor((z - calage.coinZ) / tuileM));

    const int moitie = m_nbTuiles / 2;
    const int col0   = centreCol - moitie;
    const int rang0  = centreRangee - moitie;

    /* Envoi au GPU des tuiles lues depuis la dernière image, dans la limite du
       plafond : le reste attendra l'image suivante. */
    std::vector<Prete> aEnvoyer;
    {
        std::lock_guard<std::mutex> verrou(m_mutex);
        for (int i = 0; i < TUILES_PAR_IMAGE && !m_pretes.empty(); ++i) {
            aEnvoyer.push_back(std::move(m_pretes.front()));
            m_pretes.pop_front();
        }
    }
    for (const Prete& prete : aEnvoyer) {
        envoyer(prete);
    }
    if (!aEnvoyer.empty()) {
        m_signal.notify_all(); /* de la place s'est libérée dans la file */
    }

    /* Revue des emplacements : fondu de ceux qui portent la bonne tuile,
       inventaire des manquantes pour les autres. */
    std::vector<Demande> manquantes;
    m_attendues = 0;
    for (int dr = 0; dr < m_nbTuiles; ++dr) {
        for (int dc = 0; dc < m_nbTuiles; ++dc) {
            const int    col    = col0 + dc;
            const int    rangee = rang0 + dr;
            Emplacement& e      = emplacement(col, rangee);

            if (m_pyramide.dansGrille(col, rangee)) {
                ++m_attendues;
            }
            if (e.col == col && e.rangee == rangee) {
                e.fondu = std::min(1.0f, e.fondu + dt / DUREE_FONDU_S);
                continue;
            }
            /* L'emplacement ne porte pas (encore) la tuile attendue : le shader
               ne doit pas s'en servir. */
            e.fondu = 0.0f;
            /* Hors grille : rien à charger, l'orthophoto d'ensemble suffit.
               Déjà cherchée en vain : ne pas insister. */
            const bool absente = (e.absCol == col && e.absRangee == rangee);
            if (m_pyramide.dansGrille(col, rangee) && !absente) {
                manquantes.push_back(Demande{col, rangee});
            }
        }
    }

    /* La liste des demandes est reconstruite en entier à chaque image et
       remplace la précédente : elle valait pour une position antérieure de la
       fenêtre, et une tuile qu'on aurait retirée de la file en la croyant
       demandée une fois pour toutes ne serait jamais relue. Les demandes
       partent du centre vers les bords : ce qui est sous l'appareil compte plus
       que ce qui est au loin. */
    std::sort(manquantes.begin(),
              manquantes.end(),
              [centreCol, centreRangee](const Demande& a, const Demande& b) {
                  const int da = (a.col - centreCol) * (a.col - centreCol) +
                                 (a.rangee - centreRangee) * (a.rangee - centreRangee);
                  const int db = (b.col - centreCol) * (b.col - centreCol) +
                                 (b.rangee - centreRangee) * (b.rangee - centreRangee);
                  return da < db;
              });

    {
        std::lock_guard<std::mutex> verrou(m_mutex);
        m_demandes.clear();
        for (const Demande& d : manquantes) {
            /* Déjà lue et en attente d'envoi, ou en cours de lecture : la
               redemander ferait relire le même fichier pour rien. */
            const bool enAttente =
                std::any_of(m_pretes.begin(), m_pretes.end(), [&d](const Prete& p) {
                    return p.col == d.col && p.rangee == d.rangee;
                });
            if (enAttente || (m_enLecture.col == d.col && m_enLecture.rangee == d.rangee)) {
                continue;
            }
            m_demandes.push_back(d);
        }
    }
    m_signal.notify_all();

    majResidence();
}

void Fenetre::envoyer(const Prete& prete) {
    const Calage& calage = m_pyramide.calage();
    Emplacement&  place  = emplacement(prete.col, prete.rangee);

    /* Fichier absent, abîmé, ou tuile d'un autre format que l'index : on note
       l'absence sur l'emplacement pour ne pas relancer la lecture à chaque
       image. L'orthophoto d'ensemble couvre ce carré, il n'y manquera que du
       détail. */
    if (prete.image.niveaux.empty() || prete.image.largeur != calage.tuilePx ||
        prete.image.hauteur != calage.tuilePx) {
        if (!prete.image.niveaux.empty()) {
            std::fprintf(stderr,
                         "[tuiles] tuile (%d, %d) de %d x %d px, attendu %d : ignorée.\n",
                         prete.col,
                         prete.rangee,
                         prete.image.largeur,
                         prete.image.hauteur,
                         calage.tuilePx);
        }
        place.absCol    = prete.col;
        place.absRangee = prete.rangee;
        return;
    }

    const int xBase = modulo(prete.col, m_nbTuiles) * calage.tuilePx;
    const int yBase = modulo(prete.rangee, m_nbTuiles) * calage.tuilePx;

    glBindTexture(GL_TEXTURE_2D, m_texture);
    const int niveaux = std::min<int>(NIVEAUX_FENETRE,
                                      static_cast<int>(prete.image.niveaux.size()));
    for (int i = 0; i < niveaux; ++i) {
        const dds::Niveau& n = prete.image.niveaux[static_cast<std::size_t>(i)];
        if (n.largeur < 4 || n.hauteur < 4) {
            break;  /* sous le bloc BC7 : plus alignable */
        }
        glCompressedTexSubImage2D(GL_TEXTURE_2D,
                                  i,
                                  xBase >> i,
                                  yBase >> i,
                                  n.largeur,
                                  n.hauteur,
                                  GL_COMPRESSED_RGBA_BPTC_UNORM,
                                  static_cast<GLsizei>(n.octets),
                                  prete.image.donnees.data() + n.decalage);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    place.col       = prete.col;
    place.rangee    = prete.rangee;
    place.fondu     = 0.0f;
    place.absCol    = -1;
    place.absRangee = -1;
}

void Fenetre::majResidence() {
    bool change = false;
    for (std::size_t i = 0; i < m_emplacements.size(); ++i) {
        const auto valeur =
            static_cast<unsigned char>(std::clamp(m_emplacements[i].fondu, 0.0f, 1.0f) * 255.0f);
        if (m_masque[i] != valeur) {
            m_masque[i] = valeur;
            change      = true;
        }
    }
    if (!change) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, m_residence);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    m_nbTuiles,
                    m_nbTuiles,
                    GL_RED,
                    GL_UNSIGNED_BYTE,
                    m_masque.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool Fenetre::stabilisee() const noexcept {
    if (m_texture == 0) {
        return true;  /* pas de fenêtre : rien à attendre */
    }
    int regles = 0;
    for (const Emplacement& e : m_emplacements) {
        if ((e.col >= 0 && e.fondu >= 1.0f) || e.absCol >= 0) {
            ++regles;
        }
    }
    return regles >= m_attendues;
}

int Fenetre::residentes() const noexcept {
    int n = 0;
    for (const Emplacement& e : m_emplacements) {
        if (e.col >= 0 && e.fondu >= 1.0f) {
            ++n;
        }
    }
    return n;
}

void Fenetre::bind(unsigned int uniteFenetre, unsigned int uniteResidence) const {
    glActiveTexture(GL_TEXTURE0 + uniteFenetre);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glActiveTexture(GL_TEXTURE0 + uniteResidence);
    glBindTexture(GL_TEXTURE_2D, m_residence);
}

void Fenetre::boucleLecture() {
    while (!m_fini) {
        Demande demande;
        {
            std::unique_lock<std::mutex> verrou(m_mutex);
            m_signal.wait(verrou, [this] {
                return m_fini || (!m_demandes.empty() && m_pretes.size() < PRETES_MAX);
            });
            if (m_fini) {
                return;
            }
            demande      = m_demandes.front();
            m_enLecture  = demande;  /* pour que le fil de rendu ne la redemande pas */
            m_demandes.pop_front();
        }

        /* Aucun contrôle d'empreinte : une tuile est livrée avec la carte, elle
           n'est pas dérivée d'une source locale (voir Dds.hpp). */
        auto image = dds::lire(m_pyramide.fichier(demande.col, demande.rangee));

        /* Une lecture qui échoue est signalée comme les autres, avec une image
           vide : c'est le fil de rendu qui note l'absence, faute de quoi la
           tuile serait redemandée à chaque image et un jeu de tuiles incomplet
           mitraillerait le disque. */
        std::lock_guard<std::mutex> verrou(m_mutex);
        m_enLecture = Demande{-1, -1};
        m_pretes.push_back(Prete{demande.col,
                                 demande.rangee,
                                 image.has_value() ? std::move(*image) : dds::Image{}});
    }
}

}  /* namespace artouste::render::tuiles */
