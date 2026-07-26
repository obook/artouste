/*
 * TextureCache.cpp
 * Implémentation du cache de textures BC7 (voir TextureCache.hpp).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "render/TextureCache.hpp"

#include <stb_image.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace artouste::render::cache {

namespace {

/* Empreinte courte et stable du chemin absolu de la source. Deux cartes
   peuvent porter le même nom de fichier (toutes s'appellent ortho.jpg) : le
   nom du dossier ne suffit donc pas, et deux installations du jeu côte à côte
   ne doivent pas se marcher dessus. FNV-1a suffit ici, on ne se défend contre
   aucun adversaire, seulement contre une collision fortuite. */
std::string empreinteChemin(const std::filesystem::path& chemin) {
    std::uint64_t h = 1469598103934665603ull;
    for (const char c : chemin.string()) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ull;
    }
    char tampon[17];
    std::snprintf(tampon, sizeof(tampon), "%08x", static_cast<unsigned int>(h >> 32));
    return tampon;
}

/* Nom du fichier de cache : lisible d'abord, unique ensuite. Un utilisateur qui
   ouvre son répertoire de cache doit reconnaître de quelle carte il s'agit. */
std::filesystem::path cheminCache(const std::filesystem::path& source) {
    const std::filesystem::path base = repertoire();
    if (base.empty()) {
        return {};
    }
    const std::string carte = source.parent_path().filename().string();
    return base / (carte + "-" + source.stem().string() + "-" + empreinteChemin(source) + ".dds");
}

/* Variable d'environnement, ou chaîne vide si absente ou vide. getenv renvoie
   un pointeur nul dans le premier cas et une chaîne vide dans le second, qu'on
   traite pareil : un HOME vide ne vaut pas mieux qu'un HOME absent. */
std::string variable(const char* nom) {
    const char* valeur = std::getenv(nom);
    return (valeur != nullptr) ? std::string(valeur) : std::string();
}

}  /* namespace */

std::filesystem::path repertoire() {
#ifdef _WIN32
    const std::string base = variable("LOCALAPPDATA");
    if (base.empty()) {
        return {};
    }
    return std::filesystem::path(base) / "artouste" / "orthophotos";
#else
    /* XDG_CACHE_HOME s'il est défini, sinon ~/.cache : c'est la convention de
       la spécification XDG, que respectent les outils de nettoyage système. */
    std::string base = variable("XDG_CACHE_HOME");
    if (base.empty()) {
        const std::string maison = variable("HOME");
        if (maison.empty()) {
            return {};
        }
        base = maison + "/.cache";
    }
    return std::filesystem::path(base) / "artouste" / "orthophotos";
#endif
}

Texture chargerOrthophoto(const std::filesystem::path& source,
                          const bc7::Progression&      progression) {
    const std::filesystem::path fichierCache = cheminCache(source);
    const dds::Empreinte        empreinte    = dds::empreinteDe(source);

    /* Cas courant après le premier lancement : les blocs sont prêts, il n'y a
       ni JPEG à décoder ni compression à refaire. */
    if (!fichierCache.empty()) {
        if (const auto image = dds::lire(fichierCache, empreinte)) {
            Texture texture(*image);
            if (texture.valid()) {
                std::printf("[cache] %s relu (%dx%d)\n", source.filename().string().c_str(),
                            image->largeur, image->hauteur);
                return texture;
            }
        }
    }

    /* Premier chargement de cette carte, ou source changée depuis. On décode
       ici plutôt que de laisser Texture le faire : les pixels servent à la
       compression, et on ne veut pas les décoder deux fois. */
    stbi_set_flip_vertically_on_load(1);
    int            largeur = 0, hauteur = 0, canaux = 0;
    unsigned char* pixels =
        stbi_load(source.string().c_str(), &largeur, &hauteur, &canaux, STBI_rgb_alpha);
    if (pixels == nullptr) {
        std::fprintf(stderr, "[cache] échec du chargement : %s\n", source.string().c_str());
        return Texture{};
    }

    const auto image = bc7::compresser(pixels, largeur, hauteur, progression);
    stbi_image_free(pixels);

    if (!image) {
        /* Annulé par l'utilisateur : on repart du fichier, en RGBA8. Plus
           gourmand en mémoire vidéo, mais la carte se charge quand même. */
        return Texture(source);
    }

    if (!fichierCache.empty() && !dds::ecrire(fichierCache, *image, empreinte)) {
        /* Disque plein ou répertoire non accessible en écriture : on continue
           sans cache. La carte s'affichera, elle sera seulement recompressée au
           prochain lancement. */
        std::fprintf(stderr, "[cache] écriture impossible dans %s\n",
                     fichierCache.string().c_str());
    }

    Texture texture(*image);
    if (!texture.valid()) {
        /* Pilote sans BC7 : le repli RGBA8 recharge le fichier. Le cache écrit
           juste au-dessus ne sera pas perdu pour autant, il servira sur une
           machine capable. */
        return Texture(source);
    }
    return texture;
}

}  /* namespace artouste::render::cache */
