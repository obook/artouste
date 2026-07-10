#!/usr/bin/env bash
#
# build.sh -- Compilation du simulateur Artouste sous Linux.
#
# Enchaine configuration CMake, compilation parallele, tests et, en option,
# packaging CPack. Reprend les commandes documentees dans le README mais sous
# forme d'un seul point d'entree, avec quelques options pratiques.
#
# Usage :
#   ./build.sh                  Configure et compile en Release.
#   ./build.sh -t Debug         Choisit le type de build (defaut : Release).
#   ./build.sh -c               Nettoie d'abord le repertoire de build.
#   ./build.sh -T               Lance les tests apres compilation (ctest).
#   ./build.sh -p               Produit l'archive de distribution (cpack).
#   ./build.sh -d               Compile la notice PDF (docs/notice.tex).
#   ./build.sh -r               Lance le simulateur apres compilation.
#   ./build.sh -j 4             Limite le nombre de taches paralleles.
#   ./build.sh -y               Installe les dependances manquantes sans demander.
#   ./build.sh -h               Affiche cette aide.

set -euo pipefail

# Se placer à la racine du dépôt, quel que soit le répertoire d'appel.
cd "$(dirname "$(readlink -f "$0")")" || {
    echo "ERREUR : impossible de se placer à la racine du dépôt." >&2
    exit 1
}

BUILD_DIR="build"
BUILD_TYPE="Release"
JOBS=""
DO_CLEAN=0
DO_TESTS=0
DO_PACKAGE=0
DO_DOCS=0
DO_RUN=0
ASSUME_YES=0

usage() {
    sed -n '3,19p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

# Pose une question oui/non. Renvoie 0 (oui) ou 1 (non). Avec -y, répond
# toujours oui. Sans terminal interactif et sans -y, répond non par défaut :
# aucune action lourde n'est déclenchée à l'aveugle (CI, pipe, cron).
ask_yes_no() {
    local rep
    if [ "$ASSUME_YES" -eq 1 ]; then
        return 0
    fi
    if [ ! -t 0 ]; then
        return 1
    fi
    read -rp "$1 [o/N] : " rep
    case "${rep,,}" in
        o | oui | y | yes) return 0 ;;
        *) return 1 ;;
    esac
}

while getopts ":t:j:cTpdrhy" opt; do
    case "$opt" in
        t) BUILD_TYPE="$OPTARG" ;;
        j) JOBS="$OPTARG" ;;
        c) DO_CLEAN=1 ;;
        T) DO_TESTS=1 ;;
        p) DO_PACKAGE=1 ;;
        d) DO_DOCS=1 ;;
        r) DO_RUN=1 ;;
        y) ASSUME_YES=1 ;;
        h) usage 0 ;;
        :) echo "Option -$OPTARG : argument manquant." >&2; usage 1 ;;
        \?) echo "Option inconnue : -$OPTARG." >&2; usage 1 ;;
    esac
done

# Type de build : doit correspondre à un des types acceptés par CMakeLists.txt
# (comparaison sensible à la casse, comme CMake). Une faute de frappe est
# rejetée tôt plutôt que de produire un build inattendu.
case "$BUILD_TYPE" in
    Debug | Release | RelWithDebInfo | MinSizeRel) ;;
    *)
        echo "Type de build invalide : \"$BUILD_TYPE\"." >&2
        echo "Valeurs acceptées : Debug, Release, RelWithDebInfo, MinSizeRel." >&2
        usage 1
        ;;
esac

# Nombre de tâches parallèles : valeur fournie, sinon nombre de coeurs.
if [ -z "$JOBS" ]; then
    JOBS="$(nproc 2>/dev/null || echo 1)"
fi

# La valeur de -j doit être un entier strictement positif : sinon cmake --build
# échoue plus loin avec un message peu parlant.
if ! [[ "$JOBS" =~ ^[0-9]+$ ]] || [ "$JOBS" -lt 1 ]; then
    echo "Nombre de tâches parallèles invalide : \"$JOBS\" (entier >= 1 attendu)." >&2
    usage 1
fi

echo "=============================================================="
echo " Compilation du simulateur Artouste (Alouette II)"
echo "--------------------------------------------------------------"
echo " Type de build       : $BUILD_TYPE"
echo " Répertoire de build : $BUILD_DIR"
echo " Tâches parallèles   : $JOBS"
echo " Tests               : $([ "$DO_TESTS" -eq 1 ] && echo oui || echo non)"
echo " Packaging           : $([ "$DO_PACKAGE" -eq 1 ] && echo oui || echo non)"
echo " Notice PDF          : $([ "$DO_DOCS" -eq 1 ] && echo oui || echo non)"
echo " Les dépendances tierces (GLFW, GLM, ImGui, Assimp, ...) sont"
echo " récupérées automatiquement au premier lancement : sois patient."
echo "=============================================================="

if [ "$DO_CLEAN" -eq 1 ]; then
    echo ">> Nettoyage de $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

# ---------------------------------------------------------------------------
# Dépendances système et proposition d'installation
# ---------------------------------------------------------------------------
# Plutôt que de laisser CMake ou le compilateur échouer plus loin de façon
# obscure, on vérifie tôt les outils et bibliothèques nécessaires. Si des
# éléments manquent, on propose de les installer avec le gestionnaire de
# paquets détecté (apt, dnf ou pacman). L'option -y accepte sans demander.

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

# Gestionnaire de paquets détecté (apt, dnf, pacman) ou vide si inconnu.
PKG_MGR=""
if command -v apt-get >/dev/null 2>&1; then
    PKG_MGR="apt"
elif command -v dnf >/dev/null 2>&1; then
    PKG_MGR="dnf"
elif command -v pacman >/dev/null 2>&1; then
    PKG_MGR="pacman"
fi

# Nom du ou des paquets fournissant une dépendance, selon le gestionnaire
# détecté. $1 = clé logique de la dépendance ; chaîne vide si non renseigné.
pkg_for() {
    case "$PKG_MGR:$1" in
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

collect_missing
if [ "${#MISSING_KEYS[@]}" -gt 0 ]; then
    echo ">> Dépendances manquantes pour la compilation :" >&2
    for desc in "${MISSING_DESC[@]}"; do
        echo "     - $desc" >&2
    done

    if [ -z "$PKG_MGR" ]; then
        echo "Gestionnaire de paquets non reconnu (ni apt, ni dnf, ni pacman)." >&2
        echo "Installe manuellement les éléments ci-dessus puis relance ./build.sh." >&2
        exit 1
    fi

    # Rassembler les paquets à installer, puis dédupliquer les mots (un même
    # paquet, ex. build-essential, peut couvrir plusieurs dépendances).
    pkgs=""
    for key in "${MISSING_KEYS[@]}"; do
        p="$(pkg_for "$key")"
        if [ -n "$p" ]; then
            pkgs="$pkgs $p"
        else
            echo "Paquet inconnu pour \"$key\" avec $PKG_MGR : installe-le à la main." >&2
        fi
    done
    pkgs="$(printf '%s\n' $pkgs | sort -u | tr '\n' ' ')"
    pkgs="${pkgs#"${pkgs%%[![:space:]]*}"}"
    pkgs="${pkgs%"${pkgs##*[![:space:]]}"}"

    # Commande d'installation selon le gestionnaire, préfixée par sudo si l'on
    # n'est pas déjà root.
    case "$PKG_MGR" in
        apt)    install_cmd="apt-get install $pkgs" ;;
        dnf)    install_cmd="dnf install $pkgs" ;;
        pacman) install_cmd="pacman -S --needed $pkgs" ;;
    esac
    if [ "$(id -u)" -ne 0 ]; then
        if command -v sudo >/dev/null 2>&1; then
            install_cmd="sudo $install_cmd"
        else
            echo "sudo est introuvable et tu n'es pas root." >&2
            echo "Installe les paquets en root puis relance ./build.sh :" >&2
            echo "  $install_cmd" >&2
            exit 1
        fi
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
fi

echo ">> Configuration ($BUILD_TYPE)"
# La configuration peut échouer sur un répertoire build/ obsolète : réglage figé
# dans le cache (ex. FETCHCONTENT_FULLY_DISCONNECTED) ou dépendance ajoutée
# depuis la dernière population. On propose alors de repartir d'un build propre.
if ! cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"; then
    echo "ERREUR : la configuration CMake a échoué." >&2
    if [ "$DO_CLEAN" -ne 1 ] && [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
        echo "Le répertoire $BUILD_DIR/ existait déjà : son cache est peut-être" >&2
        echo "incohérent (dépendance ajoutée depuis, réglage figé...)." >&2
        if ask_yes_no "Nettoyer $BUILD_DIR/ et reconfigurer proprement ?"; then
            echo ">> Nettoyage de $BUILD_DIR puis reconfiguration"
            rm -rf "$BUILD_DIR"
            cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
        else
            echo "Reconfiguration refusée. Relance manuellement : ./build.sh -c" >&2
            exit 1
        fi
    else
        exit 1
    fi
fi

echo ">> Compilation (-j $JOBS)"
cmake --build "$BUILD_DIR" -j "$JOBS"

if [ "$DO_TESTS" -eq 1 ]; then
    echo ">> Tests"
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

# Notice PDF : regeneree avant le packaging pour que l'archive embarque une
# version a jour. Le PDF est versionne (la CI de release ne compile pas le
# LaTeX) : apres une modif de docs/notice.tex, lance ./build.sh -d puis commite
# le PDF regenere.
if [ "$DO_DOCS" -eq 1 ]; then
    echo ">> Notice PDF (docs/notice.tex)"
    if command -v lualatex >/dev/null 2>&1; then
        # Chaine de version affichee dans la notice : derniere release (tag v*,
        # sans le prefixe v) suivie du numero de commit en decimal, c'est-a-dire
        # le nombre total de commits (compteur sequentiel), ex. 0.8.0.231. Sans
        # git ni tag, on ecrit une valeur neutre. docs/version.tex n'est pas
        # versionne : il est regenere a chaque ./build.sh -d. Le || true evite que
        # set -e n'interrompe le script quand git echoue (hors depot, sans tag).
        rel="$(git describe --tags --abbrev=0 --match 'v*' 2>/dev/null | sed 's/^v//' || true)"
        commit="$(git rev-list --count HEAD 2>/dev/null || true)"
        if [ -n "$rel" ] && [ -n "$commit" ]; then
            version="${rel}.${commit}"
        else
            version="version de développement"
        fi
        printf '%s\n' "\\newcommand{\\noticeversion}{${version}}" > docs/version.tex
        echo "   Version : $version"

        # Double passe pour stabiliser la mise en page, puis menage des fichiers
        # intermediaires. La sortie va dans un log, affiche seulement en cas d'echec.
        doc_log="docs/notice.build.log"
        if ( cd docs \
                && lualatex -interaction=nonstopmode -halt-on-error notice.tex \
                && lualatex -interaction=nonstopmode -halt-on-error notice.tex \
           ) >"$doc_log" 2>&1; then
            rm -f docs/notice.aux docs/notice.out docs/notice.log \
                  docs/notice.synctex.gz "$doc_log"
            echo "   docs/notice.pdf régénéré."
        else
            echo "   ERREUR : compilation LaTeX échouée. Voir $doc_log." >&2
            exit 1
        fi
    else
        echo "   lualatex introuvable : notice PDF non régénérée." >&2
        echo "   Installe TeX Live (paquet texlive-luatex) puis relance ./build.sh -d." >&2
    fi
fi

if [ "$DO_PACKAGE" -eq 1 ]; then
    echo ">> Packaging (cpack)"
    ( cd "$BUILD_DIR" && cpack )
fi

echo "=============================================================="
echo " Compilation terminée avec succès."
echo "--------------------------------------------------------------"
echo " Pour lancer le simulateur :"
echo "     ./$BUILD_DIR/bin/artouste"
echo " ou, en relançant ce script :"
echo "     ./build.sh -r"
echo " L'exécutable trouve le dossier assets à côté de lui."
echo "=============================================================="

if [ "$DO_RUN" -eq 1 ]; then
    ARTOUSTE_BIN="$BUILD_DIR/bin/artouste"
    if [ ! -x "$ARTOUSTE_BIN" ]; then
        echo "ERREUR : binaire introuvable ($ARTOUSTE_BIN)." >&2
        echo "La compilation a-t-elle réussi ? Relance ./build.sh sans -r pour voir." >&2
        exit 1
    fi
    echo ">> Lancement du simulateur"
    exec "$ARTOUSTE_BIN"
fi
