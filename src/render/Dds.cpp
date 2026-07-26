/*
 * Dds.cpp
 * Implémentation du conteneur DDS BC7 (voir Dds.hpp).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "render/Dds.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <system_error>

namespace artouste::render::dds {

namespace {

/* Signature du fichier et des deux en-têtes, telles que les définit Microsoft.
   Les valeurs sont écrites en petit-boutiste, l'ordre natif des plateformes
   visées (x86-64 et ARM en mode petit-boutiste). */
constexpr std::uint32_t MAGIE       = 0x20534444;  /* "DDS " */
constexpr std::uint32_t TAILLE_TETE = 124;
constexpr std::uint32_t TAILLE_PF   = 32;          /* bloc DDS_PIXELFORMAT */
constexpr std::uint32_t FOURCC_DX10 = 0x30315844;  /* "DX10" */
constexpr std::uint32_t DXGI_BC7    = 98;          /* DXGI_FORMAT_BC7_UNORM */

/* Champs dwFlags de l'en-tête : hauteur, largeur, capacités et format de
   pixel sont obligatoires ; on ajoute la taille linéaire (format compressé)
   et le nombre de mipmaps. */
constexpr std::uint32_t DDSD_OBLIGATOIRE = 0x1 | 0x2 | 0x4 | 0x1000;
constexpr std::uint32_t DDSD_LINEARSIZE  = 0x80000;
constexpr std::uint32_t DDSD_MIPMAPCOUNT = 0x20000;
constexpr std::uint32_t DDPF_FOURCC      = 0x4;
constexpr std::uint32_t DDSCAPS_TEXTURE  = 0x1000;
constexpr std::uint32_t DDSCAPS_MIPMAP   = 0x400000;
constexpr std::uint32_t DDSCAPS_COMPLEX  = 0x8;
constexpr std::uint32_t D3D10_TEXTURE2D  = 3;

/* Marque posée dans le premier mot réservé : distingue un cache écrit par
   Artouste d'un DDS BC7 quelconque, dont les octets réservés ne porteraient
   pas d'empreinte exploitable. */
constexpr std::uint32_t MARQUE = 0x4F545241;  /* "ARTO" */

/* Position des champs qu'on relit, en mots de 32 bits depuis le début du
   fichier (le mot 0 étant la signature). Nommés plutôt que calculés à la
   volée : l'en-tête DDS est un format figé, et des indices nus rendraient la
   relecture illisible. */
enum Mot : std::size_t {
    MOT_TAILLE_TETE = 1,
    MOT_HAUTEUR     = 3,
    MOT_LARGEUR     = 4,
    MOT_MIPMAPS     = 7,
    MOT_MARQUE      = 8,   /* premier des 11 mots réservés */
    MOT_EMPREINTE   = 9,   /* octets (2 mots) puis date (2 mots) */
    MOT_PF_FOURCC   = 21,
    MOT_DXGI        = 32,  /* premier mot de l'en-tête DX10 */
    NB_MOTS_ENTETE  = 36,  /* signature + 124 octets + 20 octets */
};

void ecrireMot(std::vector<std::uint32_t>& mots, std::size_t index, std::uint32_t valeur) {
    mots[index] = valeur;
}

/* Découpe une valeur 64 bits en deux mots, poids faible d'abord. */
void ecrireMot64(std::vector<std::uint32_t>& mots, std::size_t index, std::uint64_t valeur) {
    mots[index]     = static_cast<std::uint32_t>(valeur & 0xFFFFFFFFu);
    mots[index + 1] = static_cast<std::uint32_t>(valeur >> 32);
}

[[nodiscard]] std::uint64_t lireMot64(const std::vector<std::uint32_t>& mots, std::size_t index) {
    return static_cast<std::uint64_t>(mots[index]) |
           (static_cast<std::uint64_t>(mots[index + 1]) << 32);
}

}  /* namespace */

std::size_t octetsBc7(int largeur, int hauteur) noexcept {
    if (largeur <= 0 || hauteur <= 0) {
        return 0;
    }
    const std::size_t blocsX = static_cast<std::size_t>((largeur + 3) / 4);
    const std::size_t blocsY = static_cast<std::size_t>((hauteur + 3) / 4);
    return blocsX * blocsY * 16;
}

Empreinte empreinteDe(const std::filesystem::path& source) {
    std::error_code ec;
    const auto      taille = std::filesystem::file_size(source, ec);
    if (ec) {
        return {};
    }
    const auto date = std::filesystem::last_write_time(source, ec);
    if (ec) {
        return {};
    }
    /* La date est ramenée en secondes depuis l'époque de son horloge. On ne
       cherche pas une date absolue exacte, seulement une valeur stable qui
       change quand le fichier change : la comparer à elle-même suffit. */
    const auto secondes =
        std::chrono::duration_cast<std::chrono::seconds>(date.time_since_epoch()).count();
    return Empreinte{taille, static_cast<std::int64_t>(secondes)};
}

bool ecrire(const std::filesystem::path& chemin, const Image& image, const Empreinte& source) {
    if (image.niveaux.empty() || image.donnees.empty()) {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(chemin.parent_path(), ec);

    /* On écrit d'abord dans un fichier temporaire, puis on renomme : un
       lancement interrompu en plein écriture laisserait sinon un cache
       tronqué, que la relecture accepterait si seule la taille des en-têtes
       était vérifiée. Le renommage est atomique sur un même système de
       fichiers. */
    const std::filesystem::path temporaire = chemin.string() + ".part";

    std::vector<std::uint32_t> mots(NB_MOTS_ENTETE, 0u);
    ecrireMot(mots, 0, MAGIE);
    ecrireMot(mots, MOT_TAILLE_TETE, TAILLE_TETE);
    ecrireMot(mots, 2, DDSD_OBLIGATOIRE | DDSD_LINEARSIZE | DDSD_MIPMAPCOUNT);
    ecrireMot(mots, MOT_HAUTEUR, static_cast<std::uint32_t>(image.hauteur));
    ecrireMot(mots, MOT_LARGEUR, static_cast<std::uint32_t>(image.largeur));
    ecrireMot(mots, 5, static_cast<std::uint32_t>(image.niveaux.front().octets));
    ecrireMot(mots, MOT_MIPMAPS, static_cast<std::uint32_t>(image.niveaux.size()));
    ecrireMot(mots, MOT_MARQUE, MARQUE);
    ecrireMot64(mots, MOT_EMPREINTE, source.octets);
    ecrireMot64(mots, MOT_EMPREINTE + 2, static_cast<std::uint64_t>(source.date));
    ecrireMot(mots, 19, TAILLE_PF);
    ecrireMot(mots, 20, DDPF_FOURCC);
    ecrireMot(mots, MOT_PF_FOURCC, FOURCC_DX10);
    ecrireMot(mots, 27, DDSCAPS_TEXTURE | DDSCAPS_MIPMAP | DDSCAPS_COMPLEX);
    ecrireMot(mots, MOT_DXGI, DXGI_BC7);
    ecrireMot(mots, MOT_DXGI + 1, D3D10_TEXTURE2D);
    ecrireMot(mots, MOT_DXGI + 3, 1);  /* arraySize */

    {
        std::ofstream out(temporaire, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out.write(reinterpret_cast<const char*>(mots.data()),
                  static_cast<std::streamsize>(mots.size() * sizeof(std::uint32_t)));
        out.write(reinterpret_cast<const char*>(image.donnees.data()),
                  static_cast<std::streamsize>(image.donnees.size()));
        if (!out) {
            out.close();
            std::filesystem::remove(temporaire, ec);
            return false;
        }
    }

    std::filesystem::rename(temporaire, chemin, ec);
    if (ec) {
        std::filesystem::remove(temporaire, ec);
        return false;
    }
    return true;
}

std::optional<Image> lire(const std::filesystem::path& chemin, const Empreinte& attendue) {
    std::ifstream in(chemin, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }

    std::vector<std::uint32_t> mots(NB_MOTS_ENTETE, 0u);
    in.read(reinterpret_cast<char*>(mots.data()),
            static_cast<std::streamsize>(mots.size() * sizeof(std::uint32_t)));
    if (!in) {
        return std::nullopt;
    }

    /* Le fichier doit être un DDS, porter notre marque, viser BC7 par
       l'en-tête DX10, et décrire la même source que celle qu'on s'apprête à
       utiliser. Un seul de ces contrôles qui échoue et on réencode. */
    if (mots[0] != MAGIE || mots[MOT_TAILLE_TETE] != TAILLE_TETE ||
        mots[MOT_MARQUE] != MARQUE || mots[MOT_PF_FOURCC] != FOURCC_DX10 ||
        mots[MOT_DXGI] != DXGI_BC7) {
        return std::nullopt;
    }
    const Empreinte trouvee{lireMot64(mots, MOT_EMPREINTE),
                            static_cast<std::int64_t>(lireMot64(mots, MOT_EMPREINTE + 2))};
    if (!(trouvee == attendue)) {
        return std::nullopt;
    }

    Image image;
    image.largeur      = static_cast<int>(mots[MOT_LARGEUR]);
    image.hauteur      = static_cast<int>(mots[MOT_HAUTEUR]);
    const int nbNiveaux = static_cast<int>(mots[MOT_MIPMAPS]);
    if (image.largeur <= 0 || image.hauteur <= 0 || nbNiveaux <= 0 || nbNiveaux > 32) {
        return std::nullopt;
    }

    /* On reconstitue la table des niveaux à partir des dimensions : elle n'est
       pas stockée, le DDS enchaîne simplement les niveaux en divisant par deux
       à chaque fois, avec un plancher à 1 pixel. */
    std::size_t total = 0;
    int         l = image.largeur;
    int         h = image.hauteur;
    for (int i = 0; i < nbNiveaux; ++i) {
        const std::size_t octets = octetsBc7(l, h);
        image.niveaux.push_back(Niveau{l, h, total, octets});
        total += octets;
        l = std::max(1, l / 2);
        h = std::max(1, h / 2);
    }

    image.donnees.resize(total);
    in.read(reinterpret_cast<char*>(image.donnees.data()), static_cast<std::streamsize>(total));
    if (in.gcount() != static_cast<std::streamsize>(total)) {
        return std::nullopt;  /* fichier tronqué */
    }
    return image;
}

}  /* namespace artouste::render::dds */
