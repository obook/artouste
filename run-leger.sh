#!/usr/bin/env bash
#
# run-leger.sh
# Lance artouste avec une charge graphique allégée, pensée pour les machines à
# GPU intégré et les tablettes à enveloppe thermique limitée (moins de travail
# GPU = plus d'images par seconde ET moins de chaleur).
#
# On agit sur les deux postes les plus coûteux, identifiés au profilage :
#   - le budget de végétation (le poste dominant : la géométrie des arbres) ;
#   - l'anti-crénelage MSAA (bande passante mémoire).
# Aucune recompilation : ce sont des variables d'environnement lues au lancement.
# Ajustez les valeurs ci-dessous à votre goût ; plus elles sont basses, plus
# c'est fluide et frais, au prix du rendu.
#
# Auteur : O. Booklage
# Licence : GPL v2

set -euo pipefail

# Budget d'arbres soumis au GPU (defaut du simulateur : 1600000).
# 500000 est un bon compromis sur tablette ; descendez a 300000 si besoin.
export ARTOUSTE_TREE_MAX="${ARTOUSTE_TREE_MAX:-500000}"

# Anti-crenelage : 4 (defaut), 2 (allege), 0 (aucun). 2 est peu visible en 1080p.
export ARTOUSTE_MSAA="${ARTOUSTE_MSAA:-2}"

# Journal de cadence sur la sortie d'erreur (une ligne par seconde). Commentez
# cette ligne pour un lancement silencieux.
export ARTOUSTE_FPS_LOG="${ARTOUSTE_FPS_LOG:-1}"

# Repertoire du script, pour trouver le binaire quel que soit le dossier courant.
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$DIR/build/bin/artouste"

if [[ ! -x "$BIN" ]]; then
    echo "artouste n'est pas compile ($BIN introuvable). Lancez d'abord ./build.sh." >&2
    exit 1
fi

echo "Lancement allege : arbres <= $ARTOUSTE_TREE_MAX, MSAA $ARTOUSTE_MSAA." >&2
exec "$BIN" "$@"
