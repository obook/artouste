#!/usr/bin/env bash
#
# scripts/deps.sh
# Vérification et installation des dépendances système de compilation
# (compilateur, CMake, git, en-têtes OpenGL/X11/libcurl), extraites de
# build.sh. Sourcé par build.sh (qui source aussi scripts/common.sh avant,
# pour ask_yes_no/detect_pkg_mgr/prefix_sudo) ; expose ensure_build_dependencies,
# seule fonction appelée depuis build.sh.
#
# Auteur : O. Booklage
# Licence : GPL v2

# Un compilateur C++ et un outil de build (make ou ninja) sont indispensables.
have_toolchain() {
    { command -v c++ >/dev/null 2>&1 || command -v g++ >/dev/null 2>&1 \
        || command -v clang++ >/dev/null 2>&1; } \
        && { command -v make >/dev/null 2>&1 || command -v ninja >/dev/null 2>&1; }
}

have_cmake() { command -v cmake >/dev/null 2>&1; }

# git : FetchContent clone les dépendances tierces (GLFW, Assimp, flite, ...).
have_git() { command -v git >/dev/null 2>&1; }

# En-têtes OpenGL (find_package(OpenGL REQUIRED)) et X11 (GLFW compilé depuis
# les sources en a besoin). On teste un en-tête représentatif de chaque famille.
have_opengl() { [ -f /usr/include/GL/gl.h ] || [ -f /usr/local/include/GL/gl.h ]; }
have_x11()    { [ -f /usr/include/X11/Xlib.h ] || [ -f /usr/local/include/X11/Xlib.h ]; }

# libcurl (paquet de développement) : la radio internet du cockpit en dépend,
# et CMakeLists.txt s'arrête (FATAL_ERROR) si elle manque.
curl_dev_present() {
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libcurl; then
        return 0
    fi
    command -v curl-config >/dev/null 2>&1 && return 0
    for inc in /usr/include /usr/local/include; do
        [ -f "$inc/curl/curl.h" ] && return 0
    done
    return 1
}

# Nom du ou des paquets fournissant une dépendance, selon le gestionnaire
# donné ($1 = apt/dnf/pacman) et la clé logique de la dépendance ($2) ; chaîne
# vide si non renseigné.
pkg_for() {
    case "$1:$2" in
        apt:toolchain)    echo "build-essential" ;;
        apt:cmake)        echo "cmake" ;;
        apt:git)          echo "git" ;;
        apt:opengl)       echo "libgl1-mesa-dev" ;;
        apt:x11)          echo "xorg-dev" ;;
        apt:curl)         echo "libcurl4-openssl-dev" ;;
        dnf:toolchain)    echo "gcc-c++ make" ;;
        dnf:cmake)        echo "cmake" ;;
        dnf:git)          echo "git" ;;
        dnf:opengl)       echo "mesa-libGL-devel" ;;
        dnf:x11)          echo "libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel" ;;
        dnf:curl)         echo "libcurl-devel" ;;
        pacman:toolchain) echo "base-devel" ;;
        pacman:cmake)     echo "cmake" ;;
        pacman:git)       echo "git" ;;
        pacman:opengl)    echo "mesa" ;;
        pacman:x11)       echo "libx11 libxrandr libxinerama libxcursor libxi" ;;
        pacman:curl)      echo "curl" ;;
        *)                echo "" ;;
    esac
}

# Recense les dépendances manquantes dans MISSING_KEYS / MISSING_DESC. Appelée
# une première fois pour détecter, puis une seconde fois après installation.
MISSING_KEYS=()
MISSING_DESC=()
collect_missing() {
    MISSING_KEYS=()
    MISSING_DESC=()
    have_toolchain   || { MISSING_KEYS+=("toolchain"); MISSING_DESC+=("compilateur C++ et make/ninja"); }
    have_cmake       || { MISSING_KEYS+=("cmake");     MISSING_DESC+=("CMake (moteur de compilation)"); }
    have_git         || { MISSING_KEYS+=("git");       MISSING_DESC+=("git (récupération des dépendances tierces)"); }
    have_opengl      || { MISSING_KEYS+=("opengl");    MISSING_DESC+=("en-têtes de développement OpenGL"); }
    have_x11         || { MISSING_KEYS+=("x11");       MISSING_DESC+=("en-têtes de développement X11"); }
    curl_dev_present || { MISSING_KEYS+=("curl");      MISSING_DESC+=("libcurl (paquet de développement, radio internet)"); }
}

# Vérifie les dépendances système ; si certaines manquent, propose de les
# installer avec le gestionnaire de paquets détecté (apt, dnf ou pacman).
# ASSUME_YES=1 (défini par build.sh) accepte sans demander. Quitte le script
# appelant (exit 1) si l'installation est refusée, échoue, ou si aucun
# gestionnaire de paquets n'est reconnu.
ensure_build_dependencies() {
    collect_missing
    if [ "${#MISSING_KEYS[@]}" -eq 0 ]; then
        return 0
    fi

    echo ">> Dépendances manquantes pour la compilation :" >&2
    for desc in "${MISSING_DESC[@]}"; do
        echo "     - $desc" >&2
    done

    local pkg_mgr
    pkg_mgr="$(detect_pkg_mgr)"
    if [ -z "$pkg_mgr" ]; then
        echo "Gestionnaire de paquets non reconnu (ni apt, ni dnf, ni pacman)." >&2
        echo "Installe manuellement les éléments ci-dessus puis relance ./build.sh." >&2
        exit 1
    fi

    # Rassembler les paquets à installer, puis dédupliquer les mots (un même
    # paquet, ex. build-essential, peut couvrir plusieurs dépendances).
    local pkgs="" key p
    for key in "${MISSING_KEYS[@]}"; do
        p="$(pkg_for "$pkg_mgr" "$key")"
        if [ -n "$p" ]; then
            pkgs="$pkgs $p"
        else
            echo "Paquet inconnu pour \"$key\" avec $pkg_mgr : installe-le à la main." >&2
        fi
    done
    pkgs="$(printf '%s\n' $pkgs | sort -u | tr '\n' ' ')"
    pkgs="${pkgs#"${pkgs%%[![:space:]]*}"}"
    pkgs="${pkgs%"${pkgs##*[![:space:]]}"}"

    local raw_cmd install_cmd
    case "$pkg_mgr" in
        apt)    raw_cmd="apt-get install $pkgs" ;;
        dnf)    raw_cmd="dnf install $pkgs" ;;
        pacman) raw_cmd="pacman -S --needed $pkgs" ;;
    esac
    if ! install_cmd="$(prefix_sudo "$raw_cmd")"; then
        echo "sudo est introuvable et tu n'es pas root." >&2
        echo "Installe les paquets en root puis relance ./build.sh :" >&2
        echo "  $install_cmd" >&2
        exit 1
    fi

    echo >&2
    echo "Commande d'installation proposée :" >&2
    echo "  $install_cmd" >&2
    if ask_yes_no "Installer ces paquets maintenant ?"; then
        echo ">> Installation des dépendances"
        # shellcheck disable=SC2086
        if ! $install_cmd; then
            echo "ERREUR : l'installation des paquets a échoué." >&2
            echo "Corrige le problème puis relance ./build.sh." >&2
            exit 1
        fi
        # Certains paquets peuvent rester absents (nom inadapté à la distribution,
        # dépôt non à jour) : on revérifie avant de poursuivre.
        collect_missing
        if [ "${#MISSING_KEYS[@]}" -gt 0 ]; then
            echo "Des dépendances restent manquantes après installation :" >&2
            for desc in "${MISSING_DESC[@]}"; do
                echo "     - $desc" >&2
            done
            echo "Vérifie les noms de paquets pour ta distribution puis relance ./build.sh." >&2
            exit 1
        fi
    else
        echo "Installation refusée : compilation interrompue." >&2
        echo "Relance avec -y pour installer automatiquement, ou installe les" >&2
        echo "paquets ci-dessus puis relance ./build.sh." >&2
        exit 1
    fi
}
