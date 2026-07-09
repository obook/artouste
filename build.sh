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
#   ./build.sh -h               Affiche cette aide.

set -euo pipefail

# Se placer à la racine du dépôt, quel que soit le répertoire d'appel.
cd "$(dirname "$(readlink -f "$0")")"

BUILD_DIR="build"
BUILD_TYPE="Release"
JOBS=""
DO_CLEAN=0
DO_TESTS=0
DO_PACKAGE=0
DO_DOCS=0
DO_RUN=0

usage() {
    sed -n '3,18p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

while getopts ":t:j:cTpdrh" opt; do
    case "$opt" in
        t) BUILD_TYPE="$OPTARG" ;;
        j) JOBS="$OPTARG" ;;
        c) DO_CLEAN=1 ;;
        T) DO_TESTS=1 ;;
        p) DO_PACKAGE=1 ;;
        d) DO_DOCS=1 ;;
        r) DO_RUN=1 ;;
        h) usage 0 ;;
        :) echo "Option -$OPTARG : argument manquant." >&2; usage 1 ;;
        \?) echo "Option inconnue : -$OPTARG." >&2; usage 1 ;;
    esac
done

# Nombre de tâches parallèles : valeur fournie, sinon nombre de coeurs.
if [ -z "$JOBS" ]; then
    JOBS="$(nproc 2>/dev/null || echo 1)"
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

# libcurl (paquet de développement) est obligatoire : la radio internet du
# cockpit en dépend. On vérifie tôt, avec un message actionnable, plutôt que de
# laisser CMake s'arrêter plus loin de façon plus obscure.
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

if ! curl_dev_present; then
    echo "ERREUR : libcurl (paquet de développement) est introuvable." >&2
    echo "La radio internet du cockpit en a besoin : compilation interrompue." >&2
    echo "Installe-la puis relance ./build.sh :" >&2
    echo "  - Debian/Ubuntu : sudo apt install libcurl4-openssl-dev" >&2
    echo "  - Fedora        : sudo dnf install libcurl-devel" >&2
    echo "  - Arch          : sudo pacman -S curl" >&2
    exit 1
fi

echo ">> Configuration ($BUILD_TYPE)"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

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
    echo ">> Lancement du simulateur"
    exec "$BUILD_DIR/bin/artouste"
fi
