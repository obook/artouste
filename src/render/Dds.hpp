/*
 * Dds.hpp
 * Lecture et écriture du conteneur DDS pour les textures compressées BC7 du
 * cache d'orthophotos. Le format est celui de Microsoft : signature "DDS ",
 * en-tête de 124 octets, en-tête DX10 de 20 octets, puis les blocs bruts,
 * niveau de mipmap par niveau de mipmap. Un fichier écrit ici s'ouvre donc
 * dans n'importe quel visualiseur de texture, ce qui aide à déboguer.
 *
 * Le cache doit se périmer quand l'orthophoto source change (une carte
 * regénérée, un paquet de cartes réinstallé). On range pour cela une empreinte
 * de la source dans les 44 octets réservés de l'en-tête DDS, que la
 * spécification laisse inutilisés : le fichier reste un DDS valide et se
 * suffit à lui-même, sans fichier annexe à garder synchronisé.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace artouste::render::dds {

/* Ce qui identifie la version de la source. Taille et date suffisent : le
   cache est local et jetable, une collision demanderait un fichier réécrit à
   la seconde près et à l'octet près. Un condensat coûterait une relecture
   complète du JPEG à chaque lancement, soit précisément ce qu'on cherche à
   éviter. */
struct Empreinte {
    std::uint64_t octets = 0;  /* taille du fichier source */
    std::int64_t  date   = 0;  /* dernière modification, en secondes */

    [[nodiscard]] bool operator==(const Empreinte& autre) const noexcept {
        return octets == autre.octets && date == autre.date;
    }
};

/* Un niveau de mipmap dans le tampon de blocs. */
struct Niveau {
    int         largeur  = 0;
    int         hauteur  = 0;
    std::size_t decalage = 0;  /* position dans Image::donnees */
    std::size_t octets   = 0;
};

/* Une texture BC7 complète : tous les niveaux dans un seul tampon contigu. */
struct Image {
    int                        largeur = 0;
    int                        hauteur = 0;
    std::vector<Niveau>        niveaux;
    std::vector<unsigned char> donnees;
};

/* Taille en octets d'un niveau BC7 : un bloc de 16 octets par carré de 4x4
   pixels, le dernier bloc de chaque bord étant complet même si l'image ne
   remplit pas ses 4 pixels. */
[[nodiscard]] std::size_t octetsBc7(int largeur, int hauteur) noexcept;

/* Empreinte d'un fichier source. Renvoie une empreinte nulle s'il est
   illisible, ce qui invalide le cache par construction. */
[[nodiscard]] Empreinte empreinteDe(const std::filesystem::path& source);

/* Écrit l'image et son empreinte. Crée les dossiers manquants. Renvoie faux
   sur échec (disque plein, dossier non accessible en écriture) : l'appelant
   peut alors continuer sans cache plutôt que d'échouer. */
bool ecrire(const std::filesystem::path& chemin, const Image& image, const Empreinte& source);

/* Relit une image si le fichier existe, est un DDS BC7 cohérent, et porte
   l'empreinte attendue. Renvoie un optionnel vide dans tous les autres cas,
   sans distinguer "absent" de "périmé" : l'appelant réencode pareil. */
[[nodiscard]] std::optional<Image> lire(const std::filesystem::path& chemin,
                                        const Empreinte&             attendue);

}  /* namespace artouste::render::dds */
