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
       render/tuiles/Fenetre.hpp). 8192 (défaut) occupe 89 Mo de mémoire vidéo,
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

/* Lit la configuration depuis le fichier donné. Fichier absent ou clé inconnue :
   on garde les valeurs par défaut ci-dessus. */
Config loadConfig(const std::filesystem::path& path);

}  /* namespace artouste::app */
