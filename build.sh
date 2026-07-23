#!/usr/bin/env bash
#
# build.sh -- Compilation du simulateur Artouste sous Linux.
#
# Enchaîne configuration CMake, compilation parallèle, tests et, en option,
# packaging CPack. Reprend les commandes documentées dans le README mais sous
# forme d'un seul point d'entrée, avec quelques options pratiques.
#
# Usage :
#   ./build.sh                  Configure et compile en Release.
#   ./build.sh -t Debug         Choisit le type de build (défaut : Release).
#   ./build.sh -c               Nettoie d'abord le répertoire de build.
#   ./build.sh -T               Lance les tests après compilation (ctest).
#   ./build.sh -p               Produit l'archive de distribution (cpack).
#   ./build.sh -d               Compile la notice PDF (docs/notice.tex).
#   ./build.sh -r               Lance le simulateur après compilation.
#   ./build.sh -j 4             Limite le nombre de tâches parallèles.
#   ./build.sh -y               Installe les dépendances manquantes sans demander.
#   ./build.sh -h               Affiche cette aide.
#
# Auteur : O. Booklage
# Licence : GPL v2

set -euo pipefail

# Se placer à la racine du dépôt, quel que soit le répertoire d'appel : les
# scripts sourcés juste après (scripts/common.sh, deps.sh, notice.sh) sont
# alors trouvés par leur chemin relatif, sans dépendre du cwd d'origine.
cd "$(dirname "$(readlink -f "$0")")" || {
    echo "ERREUR : impossible de se placer à la racine du dépôt." >&2
    exit 1
}

source scripts/common.sh  # ask_yes_no, detect_pkg_mgr, prefix_sudo
source scripts/deps.sh    # ensure_build_dependencies
source scripts/notice.sh  # build_notice

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

# Dépendances système et proposition d'installation (scripts/deps.sh) :
# plutôt que de laisser CMake ou le compilateur échouer plus loin de façon
# obscure, on vérifie tôt les outils et bibliothèques nécessaires. Si des
# éléments manquent, on propose de les installer avec le gestionnaire de
# paquets détecté (apt, dnf ou pacman). L'option -y accepte sans demander.
ensure_build_dependencies

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

# Notice PDF (scripts/notice.sh), régénérée avant le packaging pour que
# l'archive embarque une version à jour.
if [ "$DO_DOCS" -eq 1 ]; then
    build_notice
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
