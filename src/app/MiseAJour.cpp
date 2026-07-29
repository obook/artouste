/*
 * MiseAJour.cpp
 * Implémentation de la recherche de version (voir MiseAJour.hpp).
 *
 * Comme la radio internet (audio/RadioStream.cpp), le fichier se compile en
 * deux variantes selon ARTOUSTE_HAS_CURL, réglé par src/CMakeLists.txt quand
 * libcurl est trouvée : avec, la requête réseau a lieu dans un fil ; sans, elle
 * est simplement absente. Les deux fonctions d'analyse (tag et numéros de
 * version), elles, sont toujours compilées -- elles ne dépendent de rien et
 * sont couvertes par les tests (tests/mise_a_jour_tests.cpp).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/MiseAJour.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef ARTOUSTE_HAS_CURL
#    include <curl/curl.h>
#endif

#ifdef _WIN32
/* ShellExecuteA, pour confier l'adresse au navigateur par défaut. L'ordre compte
   (shellapi.h suppose windows.h déjà lu) : on retient donc la main de
   clang-format, qui trierait les deux lignes par ordre alphabétique. */
// clang-format off
#    include <windows.h>
#    include <shellapi.h>
// clang-format on
#endif

/* Version de cet exécutable, injectée par CMake depuis le champ VERSION du
   projet (voir src/CMakeLists.txt). La valeur de repli ne sert qu'à compiler ce
   fichier hors de la cible principale, par exemple dans les tests. */
#ifndef ARTOUSTE_VERSION_SEMVER
#    define ARTOUSTE_VERSION_SEMVER "0.0.0"
#endif

namespace artouste::app {

namespace {

/* Découpe "v0.28.0.483" en majeur, mineur et correctif. Les champs absents
   valent 0, les champs au-delà du troisième sont ignorés (le compteur de commits
   de la version affichée au menu, par exemple). Faux si la chaîne ne commence pas
   par un numéro ou si un champ annoncé par un point est vide. */
bool analyserVersion(const std::string& texte, std::array<int, 3>& champs) {
    champs = {0, 0, 0};
    std::size_t i = 0;
    if (i < texte.size() && (texte[i] == 'v' || texte[i] == 'V')) {
        ++i; /* tag GitHub : "v0.28.0" */
    }
    for (int n = 0; n < 3; ++n) {
        int valeur = 0;
        bool chiffreLu = false;
        while (i < texte.size() && std::isdigit(static_cast<unsigned char>(texte[i])) != 0) {
            /* Garde-fou contre un champ absurde (débordement d'entier) : une
               chaîne pareille n'est pas un numéro de version. */
            if (valeur > 1'000'000) {
                return false;
            }
            valeur = valeur * 10 + (texte[i] - '0');
            chiffreLu = true;
            ++i;
        }
        if (!chiffreLu) {
            return false; /* rien à lire là où un nombre était attendu */
        }
        champs[static_cast<std::size_t>(n)] = valeur;
        if (i >= texte.size() || texte[i] != '.') {
            break; /* version plus courte que trois champs : le reste vaut 0 */
        }
        ++i;
    }
    return true;
}

} /* namespace */

std::string extraireTagName(const std::string& json) {
    const std::string clef = "\"tag_name\"";
    std::size_t pos = json.find(clef);
    if (pos == std::string::npos) {
        return "";
    }
    pos = json.find(':', pos + clef.size());
    if (pos == std::string::npos) {
        return "";
    }
    /* Ouverture de la chaîne de valeur, après les espaces éventuels. Une valeur
       nulle ("tag_name": null) n'en a pas : on rend une chaîne vide. */
    const std::size_t debut = json.find('"', pos + 1);
    if (debut == std::string::npos) {
        return "";
    }
    const std::size_t fin = json.find('"', debut + 1);
    if (fin == std::string::npos) {
        return "";
    }
    return json.substr(debut + 1, fin - debut - 1);
}

bool versionPlusRecente(const std::string& publiee, const std::string& locale) {
    std::array<int, 3> a{};
    std::array<int, 3> b{};
    if (!analyserVersion(publiee, a) || !analyserVersion(locale, b)) {
        return false; /* chaîne illisible : dans le doute, on ne dérange pas */
    }
    for (std::size_t n = 0; n < a.size(); ++n) {
        if (a[n] != b[n]) {
            return a[n] > b[n];
        }
    }
    return false; /* versions identiques */
}

MiseAJour::~MiseAJour() {
    if (m_fil.joinable()) {
        /* La requête est bornée par ses délais d'expiration (quelques secondes au
           pire) : l'attendre ici ne retient pas la fermeture bien longtemps, et
           garantit qu'aucun fil ne survit à l'objet qu'il écrit. */
        m_fil.join();
    }
}

bool MiseAJour::disponible() const {
    return m_disponible.load();
}

std::string MiseAJour::versionPubliee() const {
    const std::lock_guard<std::mutex> verrou(m_mutex);
    return m_version;
}

void MiseAJour::ouvrirPage() {
#if defined(_WIN32)
    /* Ouverture par le navigateur associé, sans passer par un interpréteur de
       commandes (pas de fenêtre de console qui clignote). */
    ShellExecuteA(nullptr, "open", PAGE_PROJET, nullptr, nullptr, SW_SHOWNORMAL);
#else
    /* xdg-open (open sur macOS) choisit le navigateur du bureau. L'esperluette
       rend la main tout de suite : le jeu ne doit pas attendre le démarrage du
       navigateur. L'adresse est une constante de compilation, rien d'extérieur
       n'entre dans la commande. */
#    ifdef __APPLE__
    const std::string ouvreur = "open";
#    else
    const std::string ouvreur = "xdg-open";
#    endif
    const std::string commande = ouvreur + " '" + PAGE_PROJET + "' >/dev/null 2>&1 &";
    const int code = std::system(commande.c_str());
    (void) code; /* navigateur absent : rien à faire de plus, l'adresse reste affichée */
#endif
}

#ifndef ARTOUSTE_HAS_CURL

/* --- Variante sans libcurl : pas de vérification ------------------------- */
void MiseAJour::lancer() {}

#else /* ARTOUSTE_HAS_CURL défini : requête réelle ------------------------- */

namespace {

/* Réponse attendue : quelques dizaines de kilooctets de JSON. On s'arrête bien
   avant d'avaler quoi que ce soit d'inattendu. */
constexpr std::size_t REPONSE_MAX = 512 * 1024;

/* Délais courts : la vérification est un service rendu, pas une étape du
   lancement. Passé ce temps, on renonce sans bruit. */
constexpr long DELAI_CONNEXION_S = 3;
constexpr long DELAI_TOTAL_S = 5;

std::size_t ecrireReponse(char* donnees, std::size_t taille, std::size_t nmemb, void* utilisateur) {
    auto* sortie = static_cast<std::string*>(utilisateur);
    const std::size_t octets = taille * nmemb;
    if (sortie->size() + octets > REPONSE_MAX) {
        return 0; /* trop long : curl interrompt le transfert */
    }
    sortie->append(donnees, octets);
    return octets;
}

/* Demande à GitHub le tag de la dernière release. Chaîne vide en cas d'échec
   (pas de réseau, service indisponible, réponse inattendue). */
std::string interrogerDerniereRelease() {
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        return "";
    }
    std::string reponse;
    /* En-têtes demandés par l'API de GitHub : un agent utilisateur nommé (une
       requête anonyme est refusée) et la version du format de réponse. */
    curl_slist* entetes = nullptr;
    entetes = curl_slist_append(entetes, "Accept: application/vnd.github+json");
    entetes = curl_slist_append(entetes, "X-GitHub-Api-Version: 2022-11-28");

    curl_easy_setopt(curl, CURLOPT_URL, API_DERNIERE_RELEASE);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, entetes);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "artouste/" ARTOUSTE_VERSION_SEMVER);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, DELAI_CONNEXION_S);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, DELAI_TOTAL_S);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); /* obligatoire hors du fil principal */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ecrireReponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reponse);

    const CURLcode res = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    curl_slist_free_all(entetes);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http != 200) {
        return "";
    }
    return extraireTagName(reponse);
}

} /* namespace */

void MiseAJour::lancer() {
    if (m_fil.joinable()) {
        return; /* déjà lancée */
    }
    /* Initialisation globale de libcurl faite ici, dans le fil principal : elle
       n'est pas réentrante, et la radio internet (audio/RadioStream.cpp) ouvre
       elle aussi des transferts depuis son propre fil. La libérer n'aurait pas de
       sens (la radio peut encore s'en servir) ; le système le fera à la sortie. */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    m_fil = std::thread([this]() {
        const std::string tag = interrogerDerniereRelease();
        if (tag.empty() || !versionPlusRecente(tag, ARTOUSTE_VERSION_SEMVER)) {
            return; /* à jour, ou vérification impossible : rien à signaler */
        }
        /* Le tag porte un "v" de tête ("v0.29.0") que la version locale n'a pas :
           on l'enlève pour que le menu affiche les deux numéros de la même façon. */
        const std::string publiee =
            (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) ? tag.substr(1) : tag;
        {
            const std::lock_guard<std::mutex> verrou(m_mutex);
            m_version = publiee;
        }
        m_disponible.store(true);
        std::printf("[MiseAJour] version %s disponible (vous avez %s) : %s\n",
                    publiee.c_str(),
                    ARTOUSTE_VERSION_SEMVER,
                    PAGE_PROJET);
    });
}

#endif /* ARTOUSTE_HAS_CURL */

} /* namespace artouste::app */
