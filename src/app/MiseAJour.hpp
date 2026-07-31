/*
 * MiseAJour.hpp
 * Recherche discrète d'une version plus récente au lancement, puis proposition
 * d'aller la chercher sur la page du projet.
 *
 * La version publiée est demandée à l'API de GitHub (le tag de la dernière
 * release), dans un fil séparé pour ne jamais retarder l'ouverture de la
 * fenêtre : si le réseau est absent ou lent, le menu s'affiche comme d'habitude
 * et la proposition n'apparaît simplement pas. Le résultat n'est lu qu'ensuite,
 * par le menu de démarrage (voir ApplicationMenu.cpp), qui affiche alors
 * l'adresse de la page et un bouton pour l'ouvrir dans le navigateur.
 *
 * Rien n'est envoyé au serveur : c'est une simple requête de lecture, sans
 * identifiant ni statistique. La vérification s'éteint par la clé
 * "verifier_maj 0" de config.txt (voir Config.hpp) ou par la variable
 * d'environnement ARTOUSTE_NO_MAJ.
 *
 * Sans libcurl à la compilation (ARTOUSTE_HAS_CURL non défini), la classe se
 * réduit à une coquille vide : aucune vérification, comme la radio internet
 * (voir audio/RadioStream.cpp).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace artouste::app {

/* Page de présentation du simulateur, proposée au pilote quand une version plus
   récente est publiée. Elle porte les liens de téléchargement et le numéro de la
   dernière release (voir docs/index.html). */
inline constexpr char PAGE_PROJET[] = "https://obook.github.io/artouste/";

/* Adresse interrogée pour connaître la version publiée : la dernière release du
   dépôt, dont on ne lit que le champ "tag_name" (par exemple "v0.28.0"). */
inline constexpr char API_DERNIERE_RELEASE[] =
    "https://api.github.com/repos/obook/artouste/releases/latest";

/* Extrait la valeur du champ "tag_name" de la réponse JSON de GitHub. Chaîne
   vide si le champ manque (réponse d'erreur, dépôt sans release). Une lecture
   à la main plutôt qu'une bibliothèque JSON : un seul champ, à plat. */
std::string extraireTagName(const std::string& json);

/* Vrai si la version publiée est strictement plus récente que la version locale.
   Les deux chaînes sont de la forme "0.28.0", le "v" de tête étant toléré, et les
   champs surnuméraires (le compteur de commits de ARTOUSTE_VERSION_STRING, par
   exemple) sont ignorés. Toute chaîne illisible rend faux : dans le doute, on ne
   dérange pas le pilote. */
bool versionPlusRecente(const std::string& publiee, const std::string& locale);

/*
 * Vérification lancée une fois au démarrage, consultée ensuite par le menu.
 * Un seul objet, membre de Application, dont le fil est rejoint à la
 * destruction.
 */
class MiseAJour {
public:
    MiseAJour() = default;
    ~MiseAJour();

    MiseAJour(const MiseAJour&) = delete;
    MiseAJour& operator=(const MiseAJour&) = delete;

    /* Démarre la vérification en tâche de fond. Sans effet au deuxième appel,
       comme sans libcurl. À appeler depuis le fil principal. */
    void lancer();

    /* Vrai quand une version plus récente que celle-ci a été trouvée. Tant que
       la requête est en cours, ou si elle a échoué, reste faux. */
    bool disponible() const;

    /* Numéro de la version publiée ("0.29.0"), vide tant que disponible() est
       faux. */
    std::string versionPubliee() const;

    /* Ouvre PAGE_PROJET dans le navigateur par défaut du système. Sans effet si
       le système ne sait pas honorer la demande. */
    static void ouvrirPage();

private:
    std::thread m_fil;                     /* fil de la requête réseau */
    std::atomic<bool> m_disponible{false}; /* mise à jour trouvée ? */
    mutable std::mutex m_mutex;            /* protège m_version */
    std::string m_version;                 /* version publiée, si trouvée */
};

} /* namespace artouste::app */
