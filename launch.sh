#!/usr/bin/env bash
#
# launch.sh -- Lanceur interactif du simulateur Artouste.
#
# Affiche un menu qui demande la carte a charger, puis si la turbine et le rotor
# doivent etre deja demarres au lancement. Les choix sont transmis par les
# variables d'environnement ARTOUSTE_TERRAIN et ARTOUSTE_TURBINE_DEMARREE, qui
# ont priorite sur assets/config.txt : le fichier de configuration n'est pas
# modifie.
#
# Usage :
#   ./launch.sh        Menu interactif puis lancement du simulateur.
#   ./launch.sh -n     Essai a blanc : affiche les choix sans lancer la fenetre.
#   ./launch.sh -h     Affiche cette aide.

set -euo pipefail

# Se placer à la racine du dépôt, quel que soit le répertoire d'appel.
cd "$(dirname "$(readlink -f "$0")")"

BIN="build/bin/artouste"
TERRAIN_DIR="assets/terrain"
DRY_RUN=0

case "${1:-}" in
    -n|--dry-run) DRY_RUN=1 ;;
    -h|--help) sed -n '3,14p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    "") ;;
    *) echo "Option inconnue : $1 (voir ./launch.sh -h)." >&2; exit 1 ;;
esac

# Le binaire doit avoir été compilé au préalable.
if [ ! -x "$BIN" ]; then
    echo "Le simulateur n'est pas compilé ($BIN introuvable)."
    echo "Compile-le d'abord avec : ./build.sh"
    exit 1
fi

# Découvrir les cartes : tout sous-dossier de assets/terrain/ qui contient un
# terrain.txt. Le titre lisible est extrait de la première ligne de ce fichier.
maps=()
titles=()
for d in "$TERRAIN_DIR"/*/; do
    meta="${d}terrain.txt"
    [ -f "$meta" ] || continue
    name="$(basename "$d")"
    title="$(sed -n '1s/^# Terrain Artouste - //p' "$meta")"
    maps+=("$name")
    titles+=("${title:-$name}")
done

if [ "${#maps[@]}" -eq 0 ]; then
    echo "Aucune carte trouvée dans $TERRAIN_DIR/."
    exit 1
fi

echo "=============================================================="
echo " Lanceur du simulateur Artouste (Alouette II)"
echo "=============================================================="
echo
echo "Cartes disponibles :"
for i in "${!maps[@]}"; do
    printf "  %d) %-12s %s\n" "$((i + 1))" "${maps[$i]}" "${titles[$i]}"
done
echo

# Choix de la carte (numéro), avec la première carte par défaut.
while true; do
    read -rp "Numéro de la carte [1-${#maps[@]}] (défaut 1) : " choice
    choice="${choice:-1}"
    if [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 1 ] && [ "$choice" -le "${#maps[@]}" ]; then
        break
    fi
    echo "Choix invalide, recommence."
done
terrain="${maps[$((choice - 1))]}"

# Turbine et rotor déjà au régime, ou démarrage à froid (par défaut).
echo
read -rp "Turbine et rotor déjà démarrés au lancement ? [o/N] : " rep
case "${rep,,}" in
    o | oui | y | yes) turbine=1 ;;
    *) turbine=0 ;;
esac

echo
echo "--------------------------------------------------------------"
echo " Carte            : $terrain (${titles[$((choice - 1))]})"
if [ "$turbine" -eq 1 ]; then
    echo " Turbine et rotor : démarrés (décollage immédiat)"
else
    echo " Turbine et rotor : à froid (séquence de démarrage)"
fi
echo "--------------------------------------------------------------"

export ARTOUSTE_TERRAIN="$terrain"
export ARTOUSTE_TURBINE_DEMARREE="$turbine"

if [ "$DRY_RUN" -eq 1 ]; then
    echo " (essai à blanc) Commande qui serait lancée :"
    echo "     ARTOUSTE_TERRAIN=$terrain ARTOUSTE_TURBINE_DEMARREE=$turbine $BIN"
    exit 0
fi

echo " Lancement..."
exec "$BIN"
