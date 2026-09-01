/*
 * Journal.cpp
 * Mise en oeuvre du journal (voir util/Journal.hpp).
 *
 * Les 220 diagnostics du moteur passent par printf et fprintf : on détourne
 * stdout et stderr une bonne fois vers un tube, plutôt que de toucher aux 220
 * sites d'appel. Un fil vide ce tube et recopie vers la console et le fichier.
 * Le tube s'impose parce que la bibliothèque C n'écrit un flux que vers une
 * seule destination ; le fil aussi, car un tube que personne ne lit se remplit
 * et bloque l'écrivain.
 *
 * Auteur : O. Booklage
 * Date : septembre 2026
 * Licence : GPL v2
 */

#include "util/Journal.hpp"

#include <cstdio>

#ifdef _WIN32

#include <array>
#include <cstdlib>
#include <string>
#include <system_error>
#include <thread>

#include <fcntl.h>
#include <io.h>
#include <windows.h>

namespace artouste::util {
namespace {

constexpr std::size_t TAILLE_TAMPON = 4096;

/* Console appelante, ou fichier vers lequel l'utilisateur a lui-même redirigé.
   Invalide en double-clic : seul le journal reçoit alors. */
HANDLE echo = INVALID_HANDLE_VALUE;

HANDLE fichier     = INVALID_HANDLE_VALUE;
HANDLE tubeLecture = INVALID_HANDLE_VALUE;

std::thread           filDeRecopie;
std::filesystem::path cheminRetenu;

/* Écrit tout le bloc, WriteFile pouvant n'en accepter qu'une partie. Un handle
   invalide est ignoré, ce qui laisse marcher la console sans le fichier et
   réciproquement. */
void ecrireTout(HANDLE destination, const char* octets, DWORD taille) {
    if (destination == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD ecrits = 0;
    while (ecrits < taille) {
        DWORD ecritsCeTour = 0;
        if (!WriteFile(destination, octets + ecrits, taille - ecrits, &ecritsCeTour, nullptr)) {
            return;
        }
        if (ecritsCeTour == 0) {
            return;
        }
        ecrits += ecritsCeTour;
    }
}

/* Vide le tube jusqu'à sa fermeture. ReadFile échoue sur tube rompu quand le
   destructeur referme stdout et stderr : c'est la fin normale. */
void recopier() {
    std::array<char, TAILLE_TAMPON> tampon{};
    while (true) {
        DWORD lus = 0;
        if (!ReadFile(tubeLecture, tampon.data(), static_cast<DWORD>(tampon.size()), &lus,
                      nullptr)) {
            return;
        }
        if (lus == 0) {
            return;
        }
        ecrireTout(echo, tampon.data(), lus);
        ecrireTout(fichier, tampon.data(), lus);
    }
}

/* Choisit la destination miroir. Un handle déjà valide signifie que
   l'utilisateur a redirigé lui-même ("artouste.exe > sortie.txt") : on le garde
   pour ne pas casser sa redirection. Sinon on se raccroche à la console du
   terminal appelant. En double-clic il n'y en a pas, et on n'ouvre rien. */
void preparerEcho() {
    const HANDLE deja = GetStdHandle(STD_OUTPUT_HANDLE);
    if (deja != nullptr && deja != INVALID_HANDLE_VALUE) {
        echo = deja;
        return;
    }
    if (AttachConsole(ATTACH_PARENT_PROCESS) == 0) {
        return;
    }
    /* Les sources sont en UTF-8, sans quoi les accents sortiraient en charabia. */
    SetConsoleOutputCP(CP_UTF8);
    echo = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                       OPEN_EXISTING, 0, nullptr);
}

/* _wgetenv et non getenv, pour qu'un nom d'utilisateur accentué reste lisible. */
std::filesystem::path composerChemin() {
    std::filesystem::path base;
    const wchar_t*        local = _wgetenv(L"LOCALAPPDATA");
    if (local != nullptr && local[0] != L'\0') {
        base = std::filesystem::path(local);
    } else {
        std::error_code ec;
        base = std::filesystem::temp_directory_path(ec);
        if (ec) {
            return {};
        }
    }
    return base / L"Artouste" / L"artouste.log";
}

/* Le journal précédent est mis de côté : on relance souvent le jeu juste après
   une panne, et sans ça le second lancement effacerait la trace du premier. */
void ouvrirFichier() {
    cheminRetenu = composerChemin();
    if (cheminRetenu.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(cheminRetenu.parent_path(), ec);
    if (ec) {
        cheminRetenu.clear();
        return;
    }

    std::filesystem::path precedent = cheminRetenu;
    precedent.replace_filename(L"artouste-precedent.log");
    std::filesystem::remove(precedent, ec);
    std::filesystem::rename(cheminRetenu, precedent, ec);

    fichier = CreateFileW(cheminRetenu.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fichier == INVALID_HANDLE_VALUE) {
        cheminRetenu.clear();
    }
}

/* Remplace stdout et stderr par l'entrée du tube. En double-clic le processus
   n'a aucun descripteur valide : freopen sur NUL lui en redonne un, sinon _dup2
   n'a rien sur quoi se greffer. */
bool detournerSorties() {
    HANDLE tubeEcriture = INVALID_HANDLE_VALUE;
    if (CreatePipe(&tubeLecture, &tubeEcriture, nullptr, 0) == 0) {
        return false;
    }
    if (_fileno(stdout) < 0) {
        (void)std::freopen("NUL", "w", stdout);
    }
    if (_fileno(stderr) < 0) {
        (void)std::freopen("NUL", "w", stderr);
    }

    const int descripteur =
        _open_osfhandle(reinterpret_cast<intptr_t>(tubeEcriture), _O_WRONLY | _O_BINARY);
    if (descripteur < 0) {
        CloseHandle(tubeEcriture);
        CloseHandle(tubeLecture);
        tubeLecture = INVALID_HANDLE_VALUE;
        return false;
    }
    _dup2(descripteur, _fileno(stdout));
    _dup2(descripteur, _fileno(stderr));
    _close(descripteur);

    /* Sans tampon : un plantage ne doit pas emporter les dernières lignes. */
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    return true;
}

} /* namespace */

Journal::Journal() {
    preparerEcho();
    ouvrirFichier();
    if (echo == INVALID_HANDLE_VALUE && fichier == INVALID_HANDLE_VALUE) {
        return;
    }
    if (!detournerSorties()) {
        return;
    }
    filDeRecopie = std::thread(recopier);

    if (!cheminRetenu.empty()) {
        /* Par l'écriture UTF-8 et non %ls : printf convertirait selon la locale
           du programme, celle du C, et perdrait la ligne sur un nom accentué. */
        const std::u8string brut = cheminRetenu.u8string();
        const std::string   texte(brut.begin(), brut.end());
        std::printf("[journal] sorties consignées dans %s\n", texte.c_str());
    }
}

Journal::~Journal() {
    if (filDeRecopie.joinable()) {
        /* Fermer les deux descripteurs ferme l'entrée du tube et arrête le fil.
           Tant que l'un des deux reste ouvert, il attend indéfiniment. */
        std::fflush(stdout);
        std::fflush(stderr);
        std::fclose(stdout);
        std::fclose(stderr);
        filDeRecopie.join();
    }
    /* Sans condition : le détournement a pu échouer après l'ouverture du
       fichier, auquel cas aucun fil n'a démarré. */
    if (tubeLecture != INVALID_HANDLE_VALUE) {
        CloseHandle(tubeLecture);
        tubeLecture = INVALID_HANDLE_VALUE;
    }
    if (fichier != INVALID_HANDLE_VALUE) {
        CloseHandle(fichier);
        fichier = INVALID_HANDLE_VALUE;
    }
}

std::filesystem::path Journal::chemin() noexcept {
    return cheminRetenu;
}

} /* namespace artouste::util */

#else /* !_WIN32 */

namespace artouste::util {

/* Le terminal reçoit déjà stdout et stderr : rien à détourner, et un fichier
   ferait double emploi avec la redirection du shell. */
Journal::Journal()  = default;
Journal::~Journal() = default;

std::filesystem::path Journal::chemin() noexcept {
    return {};
}

} /* namespace artouste::util */

#endif /* _WIN32 */
