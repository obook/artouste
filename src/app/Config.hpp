/*
 * Config.hpp
 * Configuration du simulateur, lue au lancement d'un fichier "clé valeur"
 * (assets/config.txt) éditable à la main. C'est le même format simple que le
 * calage du terrain (terrain.txt) : une clé et sa valeur par ligne, le caractère
 * # en début de ligne marque un commentaire. Toute clé absente garde sa valeur
 * par défaut, et un fichier manquant ne pose pas de problème.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <string>

namespace artouste::app {

struct Config {
    /* Nom du sous-dossier de assets/terrain/ à charger au lancement (par exemple
       "ossau" ou "cote-landes"). Chaque sous-dossier contient son propre
       terrain.txt, heightmap.png, ortho.jpg et landmarks.txt. */
    std::string terrain = "ossau";

    /* Démarrage immédiat : si vrai, la turbine et le rotor sont d'emblée au régime
       au lancement (au lieu de la séquence de démarrage). Pratique pour les tests,
       afin de pouvoir décoller tout de suite. */
    bool turbineRunning = false;

    /* Mode démo automatique : si vrai, l'appareil joue tout seul une démonstration
       (démarrage rapide, décollage, vol jusqu'à la Dune du Pilat survolée à environ
       1500 m, demi-tour et retour se poser sur le pad de départ), rejouée en boucle.
       La démo impose alors le terrain "arcachon". Une entrée du pilote ou la touche V
       coupe la démo. */
    bool demo = false;

    /* Végétation : si vrai (défaut), des arbres en billboards sont semés d'après
       l'orthophoto, là où le sol est vert et sous la limite forestière -- donc
       pas seulement en montagne. Mettre "arbres 0" dans config.txt pour un rendu
       léger sur une machine modeste. Cette valeur ne vaut que pour les cartes qui
       ne tranchent pas elles-mêmes (voir le options.txt d'une carte) ; la variable
       d'environnement ARTOUSTE_NO_TREES, si définie, force la désactivation. */
    bool trees = true;

    /* Recherche de mise à jour : si vrai (défaut), le simulateur demande au
       lancement, dans un fil séparé, le numéro de la dernière version publiée et
       propose au menu d'aller la chercher sur la page du projet. Rien n'est
       envoyé : c'est une simple lecture (voir MiseAJour.hpp). Mettre
       "verifier_maj 0" dans config.txt pour ne plus rien demander au réseau ; la
       variable d'environnement ARTOUSTE_NO_MAJ, si définie, force l'arrêt de la
       vérification quelle que soit cette clé. */
    bool checkUpdate = true;

    /* URL d'un flux radio internet (MP3 sur HTTP) joué dans le cockpit, allumé
       par la touche K en vol libre. Vide par défaut = pas de radio. La variable
       d'environnement ARTOUSTE_RADIO_URL, si définie, a la priorité. */
    std::string radioUrl = "";

    /* Cycle jour/nuit : vitesse d'écoulement du temps pour la course du soleil. Le
       soleil part toujours de l'heure locale du PC au lancement. Durée réelle d'une
       journée complète = 86400 / sunTimeScale secondes.
         1   -> temps réel (défaut), départ à l'heure locale du PC
         144 -> journée complète en 10 min (départ à midi)
         0   -> temps figé à midi (le soleil ne bouge pas) */
    float sunTimeScale = 1.0f;

    /* Nuit plus rapide que le jour : multiplicateur appliqué à sunTimeScale entre
       le coucher (18 h) et le lever (6 h). 2 (défaut) fait passer la nuit deux fois
       plus vite que le jour, pour ne pas rester à voler dans le noir la moitié du
       cycle ; 1 rétablit une nuit de même durée que le jour, 4 l'expédie. Sans
       effet quand le temps est figé (sunTimeScale nul). */
    float nightSpeedFactor = 2.0f;

    /* Budget de végétation : nombre maximum d'arbres soumis au GPU. Au-delà, le
       semis est éclairci uniformément. C'est le poste de rendu le plus coûteux sur
       GPU intégré : baissez-le (par exemple 500000) pour gagner des images par
       seconde et réduire la chauffe sur une machine modeste ou une tablette. La
       variable d'environnement ARTOUSTE_TREE_MAX, si définie, a la priorité. */
    int treeBudget = 1'600'000;

    /* Anti-crénelage (MSAA) : nombre d'échantillons par pixel. 4 (défaut) lisse bien
       les contours ; 2 allège la bande passante mémoire (peu visible en 1080p) ; 0
       le désactive. La variable d'environnement ARTOUSTE_MSAA, si définie, a la
       priorité. */
    int msaa = 4;

    /* Côté, en pixels, de la fenêtre de tuiles fines entretenue autour de
       l'appareil sur les cartes qui livrent un jeu de tuiles (voir
       render/tuiles/Fenetre.hpp). Attention, le rayon couvert dépend de la
       finesse des tuiles : 8192 px portent 2 km de terrain à 0,25 m/px, mais 6 km
       à 0,75. 8192 (défaut) occupe 89 Mo de mémoire vidéo,
       quelle que soit l'emprise de la carte, et couvre 2 km de terrain à
       0,25 m/px. 4096 divise cette mémoire par quatre, au prix d'un rayon de
       détail deux fois plus court : c'est le réglage d'une machine à faible
       mémoire vidéo. 0 renonce au détail fin. La variable d'environnement
       ARTOUSTE_TUILES_FENETRE, si définie, a la priorité. */
    int detailWindowPx = 8192;

    /* Dossier où sont rangés les jeux de tuiles de détail, un sous-dossier par
       carte. Par défaut "assets/terrain", c'est-à-dire chaque carte chez elle,
       dans son sous-dossier "tuiles". Un chemin absolu permet de les garder sur
       un autre disque, ce qui est souvent nécessaire : une carte fine pèse
       jusqu'à deux gigaoctets.

           tuiles_dossier /media/disque/tuiles

       Un chemin relatif est compris depuis le dossier du jeu. La variable
       d'environnement ARTOUSTE_TUILES, si définie, a la priorité. */
    std::string tilesDir = "assets/terrain";

    /* Budget de sommets du relief : nombre maximum de points de la carte
       d'altitude effectivement DESSINÉS. Au-delà, le maillage n'en retient
       qu'un sur deux, sur trois... Les altitudes restent lues en entier pour
       poser l'appareil, porter les hélisurfaces et la collision : une carte au
       relief fin améliore donc le vol même sur une machine qui ne peut pas tout
       dessiner. Doubler la finesse du maillage quadruple le nombre de
       triangles, c'est le second levier de performance après les arbres.
       1200000 (défaut) correspond au maillage des cartes actuelles ; descendre
       vers 300000 sur une machine modeste. 0 dessine tout. La variable
       d'environnement ARTOUSTE_RELIEF_SOMMETS, si définie, a la priorité. */
    int reliefVertexBudget = 1'200'000;
};

/* Clés que ce chargeur sait lire, et elles seules. C'est la référence du
   programme, opposée au fichier modèle config.default.txt, qui n'est que du
   texte sur le disque : un modèle effacé, abîmé ou remplacé par autre chose ne
   doit jamais faire écrire n'importe quoi dans la configuration personnelle de
   l'utilisateur. Toute option recopiée du modèle est donc filtrée par cette
   liste (voir loadConfig). Les tests vérifient qu'elle correspond exactement aux
   clés documentées dans le modèle, dans les deux sens : une option ajoutée au
   code mais oubliée dans le modèle ne serait jamais proposée à personne, et une
   option décrite par le modèle mais inconnue du code serait recopiée puis
   rejetée à la lecture suivante. */
const std::set<std::string>& clesConnues();

/* Options renommées au fil des versions : ancien nom -> nom actuel. Renommer une
   clé sans cette table serait une petite trahison : le fichier de l'utilisateur
   garderait l'ancien nom, que le simulateur rejetterait comme inconnu, et son
   réglage -- des arbres coupés sur une machine modeste, par exemple -- repasserait
   en silence au défaut du nouveau nom. Le chargeur renomme donc la clé dans le
   fichier lui-même, en gardant la valeur choisie (voir loadConfig).

   Une entrée ne se retire jamais : elle sert aux fichiers d'une version
   ancienne, quel que soit son âge. Les tests vérifient que chaque nom actuel
   figure dans clesConnues() et qu'aucun ancien nom n'y figure encore. */
const std::map<std::string, std::string>& clesRenommees();

/* Remplace dans le fichier donné les clés portant un ancien nom par leur nom
   actuel, en conservant la valeur, la position et le reste de la ligne. Si le
   fichier porte déjà le nom actuel, l'ancienne ligne est neutralisée (mise en
   commentaire) plutôt que dupliquée. Rend le nombre de lignes modifiées.

   Appelée par loadConfig avec clesRenommees() ; le paramètre existe pour que les
   tests puissent lui soumettre leur propre table. */
std::size_t renommerAnciennesCles(const std::filesystem::path& config,
                                  const std::map<std::string, std::string>& renommages);

/* Lit la configuration depuis le fichier donné. Fichier absent ou clé inconnue :
   on garde les valeurs par défaut ci-dessus.

   Au passage, la configuration personnelle est complétée : les options que le
   modèle config.default.txt (rangé à côté) documente et qu'elle n'a pas -- celles
   qu'une nouvelle version du simulateur apporte -- sont recopiées à la fin du
   fichier, commentaires compris, avec la valeur d'une installation neuve. Les
   réglages existants ne sont jamais réécrits. */
Config loadConfig(const std::filesystem::path& path);

} /* namespace artouste::app */
