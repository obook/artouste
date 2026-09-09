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

# Système hôte : "Darwin" sous macOS, "Linux" ailleurs. Plusieurs vérifications
# n'ont pas le même sens des deux côtés : macOS n'a ni X11, ni en-têtes OpenGL
# dans /usr/include (tout vient du SDK), et son compilateur s'installe avec les
# outils en ligne de commande de Xcode et non par un gestionnaire de paquets.
SYSTEME="$(uname -s)"

# Un compilateur C++ et un outil de build (make ou ninja) sont indispensables.
# Sous macOS, /usr/bin/c++ existe même sans compilateur : c'est une amorce qui
# ouvre une fenêtre d'installation quand on l'appelle. On interroge donc
# xcode-select, seul témoin fiable de la présence des outils Xcode.
have_toolchain() {
    if [ "$SYSTEME" = "Darwin" ]; then
        xcode-select -p >/dev/null 2>&1
        return
    fi
    { command -v c++ >/dev/null 2>&1 || command -v g++ >/dev/null 2>&1 \
        || command -v clang++ >/dev/null 2>&1; } \
        && { command -v make >/dev/null 2>&1 || command -v ninja >/dev/null 2>&1; }
}

have_cmake() { command -v cmake >/dev/null 2>&1; }

# git : FetchContent clone les dépendances tierces (GLFW, Assimp, flite, ...).
have_git() { command -v git >/dev/null 2>&1; }

# En-têtes OpenGL (find_package(OpenGL REQUIRED)) et X11 (GLFW compilé depuis
# les sources en a besoin). On teste un en-tête représentatif de chaque famille.
# Sous macOS, les deux sont sans objet : OpenGL est fourni par le SDK livré avec
# Xcode (framework OpenGL, déprécié mais toujours là, et 4.1 suffit au moteur) et
# GLFW s'appuie sur Cocoa, pas sur X11. On répond donc oui sans rien chercher.
have_opengl() {
    [ "$SYSTEME" = "Darwin" ] && return 0
    [ -f /usr/include/GL/gl.h ] || [ -f /usr/local/include/GL/gl.h ]
}
have_x11() {
    [ "$SYSTEME" = "Darwin" ] && return 0
    [ -f /usr/include/X11/Xlib.h ] || [ -f /usr/local/include/X11/Xlib.h ]
}

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
        # brew ne fournit pas de compilateur : voir conseils_macos ci-dessous.
        brew:cmake)       echo "cmake" ;;
        brew:git)         echo "git" ;;
        brew:curl)        echo "curl" ;;
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

# Marche à suivre complète sous macOS. Elle s'affiche dès qu'une dépendance
# manque, parce que rien ne s'y installe par un canal unique : le compilateur
# vient de Xcode, CMake et git de Homebrew, libcurl du système.
conseils_macos() {
    echo >&2
    echo "-----------------------------------------------------------------" >&2
    echo "Que faire sous macOS :" >&2
    echo >&2
    echo "  1. Compilateur C++ et make -- outils en ligne de commande Xcode :" >&2
    echo "         xcode-select --install" >&2
    echo "     Une fenêtre s'ouvre et demande confirmation ; le téléchargement" >&2
    echo "     prend quelques minutes. Pour vérifier ensuite : xcode-select -p" >&2
    echo >&2
    echo "  2. CMake et git -- par Homebrew (https://brew.sh) :" >&2
    echo "         brew install cmake git" >&2
    echo "     Si la commande brew est introuvable, installe d'abord Homebrew :" >&2
    echo '         /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"' >&2
    echo "     puis ajoute-le au PATH comme l'indique la fin de son installation" >&2
    echo '         eval "$(/opt/homebrew/bin/brew shellenv)"   # puces Apple' >&2
    echo '         eval "$(/usr/local/bin/brew shellenv)"      # Mac Intel' >&2
    echo "     Ne lance JAMAIS brew avec sudo : il refuse de tourner en root." >&2
    echo >&2
    echo "  3. libcurl, OpenGL et l'affichage : rien à installer. libcurl et le" >&2
    echo "     framework OpenGL viennent du système, et la fenêtre passe par" >&2
    echo "     Cocoa (X11 n'est pas utilisé)." >&2
    echo >&2
    echo "Cela fait, relance ./build.sh depuis la racine du dépôt." >&2
    echo "-----------------------------------------------------------------" >&2
}

# Vérifie les dépendances système ; si certaines manquent, propose de les
# installer avec le gestionnaire de paquets détecté (apt, dnf, pacman ou brew).
# ASSUME_YES=1 (défini par build.sh) accepte sans demander. Quitte le script
# appelant (exit 1) si l'installation est refusée, échoue, ou si aucun
# gestionnaire de paquets n'est reconnu.
ensure_build_dependencies() {
    collect_missing
    # Tableau vide testé par "${tableau[*]+x}" et non par sa longueur : sous
    # bash 3.2, seul bash livré par macOS, "${#tableau[@]}" sur un tableau vide
    # déclenche une erreur "unbound variable" à cause du set -u de build.sh.
    if [ -z "${MISSING_KEYS[*]+x}" ]; then
        return 0
    fi

    echo ">> Dépendances manquantes pour la compilation :" >&2
    for desc in "${MISSING_DESC[@]}"; do
        echo "     - $desc" >&2
    done

    local pkg_mgr
    pkg_mgr="$(detect_pkg_mgr)"

    # macOS : deux cas qu'aucune commande d'installation ne réglerait seule --
    # le compilateur Xcode, qui ne s'installe pas par un gestionnaire de
    # paquets, et Homebrew lui-même absent. On déroule la marche à suivre.
    if [ "$SYSTEME" = "Darwin" ] \
        && { ! have_toolchain || [ "$pkg_mgr" != "brew" ]; }; then
        conseils_macos
        exit 1
    fi

    if [ -z "$pkg_mgr" ]; then
        echo "Gestionnaire de paquets non reconnu (ni apt, ni dnf, ni pacman, ni brew)." >&2
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
        brew)   raw_cmd="brew install $pkgs" ;;
    esac
    # Homebrew installe dans un préfixe qui appartient à l'utilisateur et refuse
    # de tourner en root : pas de sudo devant lui.
    if [ "$pkg_mgr" = "brew" ]; then
        install_cmd="$raw_cmd"
    elif ! install_cmd="$(prefix_sudo "$raw_cmd")"; then
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
        if [ -n "${MISSING_KEYS[*]+x}" ]; then
            echo "Des dépendances restent manquantes après installation :" >&2
            for desc in "${MISSING_DESC[@]}"; do
                echo "     - $desc" >&2
            done
            if [ "$SYSTEME" = "Darwin" ]; then
                conseils_macos
            else
                echo "Vérifie les noms de paquets pour ta distribution puis relance ./build.sh." >&2
            fi
            exit 1
        fi
    else
        echo "Installation refusée : compilation interrompue." >&2
        echo "Relance avec -y pour installer automatiquement, ou installe les" >&2
        echo "paquets ci-dessus puis relance ./build.sh." >&2
        exit 1
    fi
}
