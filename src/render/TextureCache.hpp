/*
 * TextureCache.hpp
 * Cache local de textures BC7 pour les orthophotos de terrain.
 *
 * Les cartes sont livrées en JPEG, format compact à télécharger mais que le
 * GPU ne sait pas lire : il faut le décoder en RGBA, quatre octets par pixel.
 * Sur une carte fine cela représente des centaines de mégaoctets de mémoire
 * vidéo, et c'est ce budget qui plafonne la finesse au sol qu'on peut se
 * permettre. BC7 divise la note par quatre pour une perte indiscernable, mais
 * compresser coûte du temps : une quarantaine de secondes sur une carte de
 * 94 mégapixels.
 *
 * D'où ce cache : on compresse une fois, au premier chargement de la carte, et
 * on range le résultat à côté. Les lancements suivants ne font plus que lire
 * des blocs déjà prêts, ce qui est plus rapide que le chemin d'origine
 * puisqu'on économise aussi le décodage du JPEG.
 *
 * Le cache vit dans le répertoire de cache de l'utilisateur, jamais dans
 * assets/ : le jeu peut être installé en lecture seule, et un cache est par
 * nature jetable, à ne pas sauvegarder ni synchroniser.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include <filesystem>

#include "render/Bc7.hpp"
#include "render/Texture.hpp"

namespace artouste::render::cache {

/* Répertoire où sont rangées les textures préparées. Suit la convention de
   chaque système : XDG_CACHE_HOME ou ~/.cache sous Linux, LOCALAPPDATA sous
   Windows. Renvoie un chemin vide si aucun n'est déterminable, ce qui désactive
   simplement le cache. */
[[nodiscard]] std::filesystem::path repertoire();

/* Charge une orthophoto en texture, via le cache.

   Si le cache est à jour, les blocs sont envoyés directement au GPU et le
   rappel n'est jamais appelé. Sinon l'image est décodée, compressée en
   alimentant le rappel, puis rangée pour la prochaine fois.

   Le rappel peut renvoyer faux pour annuler : on retombe alors sur un
   chargement RGBA8 non compressé, plus gourmand en mémoire mais immédiat. Même
   repli si le pilote ne gère pas BC7, si l'écriture du cache échoue, ou si le
   répertoire de cache est indisponible : dans tous ces cas le jeu se lance,
   seulement moins bien. */
[[nodiscard]] Texture chargerOrthophoto(const std::filesystem::path& source,
                                        const bc7::Progression&      progression);

}  /* namespace artouste::render::cache */
