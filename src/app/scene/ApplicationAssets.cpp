/*
 * ApplicationAssets.cpp
 * Localisation du dossier des ressources ("assets") et vérification de sa
 * présence, indépendamment de toute ressource GL : amorçage pur système de
 * fichiers, appelé avant initScene() (voir ApplicationScene.cpp).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

/* Sous Windows, pour retrouver le dossier du binaire (GetModuleFileNameW).
 * WIN32_LEAN_AND_MEAN et NOMINMAX évitent que <windows.h> tire des macros
 * min/max qui entreraient en conflit avec GLM et la bibliothèque standard. */
#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

#include "app/Application.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#ifndef ARTOUSTE_ASSET_DIR
#define ARTOUSTE_ASSET_DIR "assets"
#endif

namespace artouste::app {

namespace {

/* Dossier contenant le binaire en cours d'exécution, ou un chemin vide si on ne
 * sait pas le déterminer. Sert à trouver les ressources installées à côté du
 * binaire (cas d'une release). Portable : GetModuleFileNameW sous Windows,
 * /proc/self/exe sous Linux. */
std::filesystem::path executableDir() {
    namespace fs = std::filesystem;
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        return fs::path(buf, buf + n).parent_path();
    }
#else
    std::error_code ec;
    const fs::path  exe = fs::canonical("/proc/self/exe", ec);
    if (!ec) {
        return exe.parent_path();
    }
#endif
    return fs::path();
}

/*
 * Signale l'absence des ressources par le canal adapté à la plateforme. L'exe
 * Windows est une application fenêtrée (pas de console) : un simple printf serait
 * invisible pour l'utilisateur. On affiche donc une vraie boîte de dialogue native
 * qui explique la manoeuvre. Ailleurs (lancement depuis un terminal), un message
 * sur la sortie d'erreur suffit.
 */
void signalerAssetsManquants(const std::filesystem::path& cherche) {
#if defined(_WIN32)
    (void)cherche;
    const wchar_t* titre = L"Artouste -- fichiers manquants";
    const wchar_t* corps =
        L"Le simulateur n'a pas trouvé ses fichiers (dossier \"assets\").\n\n"
        L"Tu l'as probablement lancé depuis l'intérieur du zip.\n"
        L"Il faut d'abord EXTRAIRE l'archive :\n\n"
        L"    1. Clic droit sur le fichier .zip\n"
        L"    2. \"Extraire tout...\"\n"
        L"    3. Ouvrir le dossier extrait\n"
        L"    4. Double-cliquer sur artouste.exe";
    MessageBoxW(nullptr, corps, titre, MB_OK | MB_ICONERROR);
#else
    std::fprintf(stderr,
                 "Artouste : dossier \"assets\" introuvable (cherché : %s).\n"
                 "Lance le simulateur depuis le dossier qui contient \"assets\",\n"
                 "ou définis la variable d'environnement ARTOUSTE_ASSETS.\n",
                 cherche.string().c_str());
#endif
}

}  /* namespace */

/*
 * Localise le dossier des ressources, dans l'ordre : variable d'environnement,
 * puis dossier "assets" placé à côté de l'exécutable (version packagée), puis
 * chemin connu à la compilation (développement).
 */
std::filesystem::path Application::resolveAssetDir() {
    namespace fs = std::filesystem;
    if (const char* env = std::getenv("ARTOUSTE_ASSETS")) {
        if (fs::exists(env)) {
            return fs::path(env);
        }
    }
    /* Ressources installées à côté du binaire (release) : prioritaires sur le
       chemin de compilation, qui n'existe que sur la machine de développement. */
    const fs::path exeDir = executableDir();
    if (!exeDir.empty()) {
        const fs::path local = exeDir / "assets";
        if (fs::exists(local)) {
            return local;
        }
    }
    if (fs::exists(ARTOUSTE_ASSET_DIR)) {
        return fs::path(ARTOUSTE_ASSET_DIR);
    }
    return fs::path("assets");
}

bool Application::assetsDisponibles() {
    namespace fs = std::filesystem;
    const fs::path assets = resolveAssetDir();
    /* Sentinelle : le sous-dossier des shaders est indispensable et toujours présent
       dans une installation correcte. Son absence signe des ressources non extraites
       ou introuvables (exe lancé seul, depuis le zip, ou déplacé sans son "assets"). */
    if (fs::exists(assets / "shaders")) {
        return true;
    }
    signalerAssetsManquants(assets);
    return false;
}

}  /* namespace artouste::app */
