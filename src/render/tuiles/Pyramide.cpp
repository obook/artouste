/*
 * Pyramide.cpp
 * Lecture et écriture de l'index d'un jeu de tuiles, et passage des coordonnées
 * monde aux indices de tuile (voir Pyramide.hpp).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "render/tuiles/Pyramide.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace artouste::render::tuiles {

std::optional<Pyramide> Pyramide::ouvrir(const std::filesystem::path& dossier) {
    std::ifstream in(dossier / NOM_INDEX);
    if (!in) {
        return std::nullopt;
    }

    /* Même convention que terrain.txt : une clé par ligne, # en commentaire,
       clé inconnue ignorée avec sa valeur (un index écrit par une version plus
       récente reste lisible). */
    Calage      calage;
    std::string cle;
    while (in >> cle) {
        if (!cle.empty() && cle[0] == '#') {
            std::getline(in, cle);
            continue;
        }
        if (cle == "tuile_px") {
            in >> calage.tuilePx;
        } else if (cle == "m_par_pixel") {
            in >> calage.mParPixel;
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

    if (!calage.valide()) {
        return std::nullopt;
    }
    return Pyramide{dossier, calage};
}

bool Pyramide::ecrireIndex() const {
    std::error_code ec;
    std::filesystem::create_directories(m_dossier, ec);

    std::ofstream out(m_dossier / NOM_INDEX, std::ios::trunc);
    if (!out) {
        return false;
    }
    out << "# Tuiles de détail Artouste - " << m_dossier.filename().string() << "\n";
    out << "# Grille ancrée sur le coin nord-ouest de la tuile (0, 0), en coordonnées\n";
    out << "# monde (X est, Z sud). Une tuile par fichier BC7 : <rangée>/<colonne>.dds\n";
    out << "tuile_px " << m_calage.tuilePx << "\n";
    out << "m_par_pixel " << m_calage.mParPixel << "\n";
    out << "colonnes " << m_calage.colonnes << "\n";
    out << "rangees " << m_calage.rangees << "\n";
    out << "coin_x " << m_calage.coinX << "\n";
    out << "coin_z " << m_calage.coinZ << "\n";
    return out.good();
}

bool Pyramide::tuileEn(float x, float z, int& col, int& rangee) const noexcept {
    const float pas = m_calage.tuileM();
    if (pas <= 0.0f) {
        return false;
    }
    /* Division plancher : un point à l'ouest ou au nord du coin d'ancrage donne
       un indice négatif, donc hors grille, et non la tuile 0 par troncature. */
    const float fc = std::floor((x - m_calage.coinX) / pas);
    const float fr = std::floor((z - m_calage.coinZ) / pas);
    if (fc < 0.0f || fr < 0.0f || fc >= static_cast<float>(m_calage.colonnes) ||
        fr >= static_cast<float>(m_calage.rangees)) {
        return false;
    }
    col    = static_cast<int>(fc);
    rangee = static_cast<int>(fr);
    return true;
}

void Pyramide::coinTuile(int col, int rangee, float& x, float& z) const noexcept {
    const float pas = m_calage.tuileM();
    x = m_calage.coinX + static_cast<float>(col) * pas;
    z = m_calage.coinZ + static_cast<float>(rangee) * pas;
}

std::filesystem::path Pyramide::fichier(int col, int rangee) const {
    return m_dossier / std::to_string(rangee) / (std::to_string(col) + ".dds");
}

std::filesystem::path cheminJeuDeTuiles(const std::filesystem::path& dossierCarte,
                                        const std::filesystem::path& racine) {
    /* Racines à explorer, dans l'ordre de priorité, chacune pouvant ranger le jeu
       de tuiles directement ou sous un sous-dossier "tuiles". */
    std::vector<std::filesystem::path> candidats;
    const auto ajouterRacine = [&candidats, &dossierCarte](const std::filesystem::path& base) {
        if (base.empty()) {
            return;
        }
        candidats.push_back(base / dossierCarte.filename());
        candidats.push_back(base / dossierCarte.filename() / "tuiles");
    };

    if (const char* env = std::getenv("ARTOUSTE_TUILES"); env != nullptr && env[0] != '\0') {
        ajouterRacine(env);
    }
    ajouterRacine(racine);
    candidats.push_back(dossierCarte / "tuiles");

    std::error_code ec;
    for (const std::filesystem::path& candidat : candidats) {
        /* Un dossier ne compte que s'il porte au moins un index, à la racine ou
           dans un sous-dossier de niveau : sinon c'est un reste de génération
           interrompue, pas un jeu de tuiles. */
        if (std::filesystem::is_directory(candidat, ec) && !ouvrirNiveaux(candidat).empty()) {
            return candidat;
        }
    }
    return {};
}

std::vector<Pyramide> ouvrirNiveaux(const std::filesystem::path& dossier) {
    std::vector<Pyramide> niveaux;
    if (auto racine = Pyramide::ouvrir(dossier)) {
        niveaux.push_back(std::move(*racine));
    }

    /* Un sous-dossier est un niveau s'il porte un index, et seulement à ce
       titre : les dossiers de rangées de tuiles (0, 1, 2...) et les traces de
       génération n'en ont pas et sont donc ignorés sans avoir à les nommer. */
    std::error_code ec;
    for (const auto& entree : std::filesystem::directory_iterator(dossier, ec)) {
        if (!entree.is_directory(ec)) {
            continue;
        }
        if (auto niveau = Pyramide::ouvrir(entree.path())) {
            niveaux.push_back(std::move(*niveau));
        }
    }

    std::sort(niveaux.begin(), niveaux.end(), [](const Pyramide& a, const Pyramide& b) {
        return a.calage().mParPixel > b.calage().mParPixel;
    });
    return niveaux;
}

}  /* namespace artouste::render::tuiles */
