#!/usr/bin/env bash
#
# play-linux.sh
# Recompile puis lance artouste, en passant par gamemode si disponible
# (le démon gamemoded optimise le CPU/GPU pendant la session de jeu).
# Sans gamemode installé, propose l'installation ; en cas de refus ou
# d'échec, lance le simulateur normalement.
#
# Usage :
#   ./play-linux.sh          Recompile puis lance (propose l'installation de gamemode).
#   ./play-linux.sh -y       Installe gamemode sans demander si absent.
#
# Auteur : O. Booklage
# Licence : GPL v2

set -euo pipefail

# Se placer à la racine du dépôt, quel que soit le répertoire d'appel : le
# script sourcé juste après (scripts/common.sh) est alors trouvé par son
# chemin relatif, sans dépendre du cwd d'origine.
cd "$(dirname "$(readlink -f "$0")")" || {
    echo "ERREUR : impossible de se placer à la racine du dépôt." >&2
    exit 1
}

source scripts/common.sh  # ask_yes_no, detect_pkg_mgr, prefix_sudo

ASSUME_YES=0
while getopts ":yh" opt; do
    case "$opt" in
        y) ASSUME_YES=1 ;;
        h)
            sed -n '3,12p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        \?) echo "Option inconnue : -$OPTARG." >&2; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------
# Cellule 3dexport : modèle commercial, absent du dépôt public (licence). On
# la met à jour depuis le dépôt privé obook/artouste-assets, en ne téléchargeant
# que si son empreinte git diffère de la copie locale. Tout échec (réseau, gh
# absent ou non connecté, accès refusé) est sans gravité : la copie locale
# sert, et à défaut le simulateur reprend le modèle libre alouette.ac.
# ---------------------------------------------------------------------------

CELLULE="assets/models/Alouette-II/Models/alouette-3dexport.glb"
CELLULE_API="repos/obook/artouste-assets/contents/alouette-3dexport.glb"

maj_cellule() {
    command -v gh >/dev/null 2>&1 || return 1
    local distant local_ tmp
    distant="$(timeout 30 gh api "$CELLULE_API" --jq .sha 2>/dev/null)" || return 1
    [ -n "$distant" ] || return 1
    local_="$(git hash-object "$CELLULE" 2>/dev/null || true)"
    if [ "$distant" = "$local_" ]; then
        echo "Cellule 3dexport à jour."
        return 0
    fi
    tmp="$(mktemp "$CELLULE.XXXXXX")"
    if timeout 300 gh api -H "Accept: application/vnd.github.raw" "$CELLULE_API" >"$tmp" 2>/dev/null \
        && [ "$(git hash-object "$tmp")" = "$distant" ]; then
        mv "$tmp" "$CELLULE"
        echo "Cellule 3dexport mise à jour."
        return 0
    fi
    rm -f "$tmp"
    return 1
}

echo ">> Récupération de la cellule 3dexport"
if ! maj_cellule; then
    echo "AVERTISSEMENT : cellule 3dexport non récupérée depuis GitHub" >&2
    echo "(réseau coupé, gh absent ou non connecté, ou accès refusé)." >&2
    if [ -f "$CELLULE" ]; then
        echo "-> Repli sur la copie locale, peut-être ancienne : $CELLULE" >&2
    else
        echo "-> Repli sur le modèle libre alouette.ac." >&2
    fi
fi

echo ">> Compilation"
if [ "$ASSUME_YES" -eq 1 ]; then
    ./build.sh -y
else
    ./build.sh
fi

BIN="build/bin/artouste"
if [ ! -x "$BIN" ]; then
    echo "ERREUR : binaire introuvable ($BIN)." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Carte graphique : sur un portable Optimus, le rendu part par défaut sur la
# puce intégrée et le simulateur plafonne à 30 fps. Ces variables le confient
# à la carte NVIDIA. Elles sont sans effet sur une machine où celle-ci assure
# déjà le rendu, et ne sont posées que si son pilote répond.
# ---------------------------------------------------------------------------

VENDEUR_EGL_NVIDIA="/usr/share/glvnd/egl_vendor.d/10_nvidia.json"

# nvidia-smi est livré avec le pilote propriétaire : son absence signale une
# machine sans carte NVIDIA, ou sous nouveau, où ces variables ne serviraient
# à rien. L'exécuter réellement vérifie en plus que le module est chargé, le
# binaire pouvant subsister après une mise à jour ratée.
if [ "${ARTOUSTE_SANS_NVIDIA:-0}" -ne 0 ]; then
    echo ">> Rendu laissé à la carte par défaut (ARTOUSTE_SANS_NVIDIA)"

elif command -v nvidia-smi >/dev/null 2>&1 && nvidia-smi >/dev/null 2>&1; then
    echo ">> Rendu confié à la carte NVIDIA (pilote propriétaire)"
    export __NV_PRIME_RENDER_OFFLOAD=1
    export __GLX_VENDOR_LIBRARY_NAME=nvidia

    # Le rendu EGL, utilisé par les applications Wayland natives, ignore la
    # variable GLX ci-dessus et réclame le chemin du pilote.
    if [ -f "$VENDEUR_EGL_NVIDIA" ]; then
        export __EGL_VENDOR_LIBRARY_FILENAMES="$VENDEUR_EGL_NVIDIA"
    fi

elif [ -e /dev/dri/renderD129 ]; then
    # Pile Mesa : carte AMD dédiée, ou NVIDIA sous nouveau ou NVK. Le pilote
    # propriétaire est absent, donc les variables ci-dessus n'ont aucun effet :
    # c'est DRI_PRIME qui commande le déport. Un second noeud de rendu DRI
    # signale la présence d'une seconde carte.
    echo ">> Rendu confié à la carte dédiée (Mesa)"
    export DRI_PRIME=1
fi

# ---------------------------------------------------------------------------
# gamemode : optimise CPU/GPU pendant la session (governor performance, I/O
# priorité, etc). Facultatif : le jeu tourne tout aussi bien sans lui.
# ---------------------------------------------------------------------------

echo ">> Vérification de gamemode"
if ! command -v gamemoderun >/dev/null 2>&1; then
    echo "gamemode n'est pas installé." >&2

    PKG_MGR="$(detect_pkg_mgr)"
    case "$PKG_MGR" in
        apt)    INSTALL_CMD="apt-get install gamemode" ;;
        dnf)    INSTALL_CMD="dnf install gamemode" ;;
        pacman) INSTALL_CMD="pacman -S --needed gamemode" ;;
    esac

    if [ -z "$PKG_MGR" ]; then
        echo "Gestionnaire de paquets non reconnu (ni apt, ni dnf, ni pacman)." >&2
        echo "Installe gamemode manuellement si tu le souhaites." >&2
    else
        if ! INSTALL_CMD="$(prefix_sudo "$INSTALL_CMD")"; then
            echo "sudo est introuvable et tu n'es pas root : installation impossible." >&2
            INSTALL_CMD=""
        fi

        if [ -n "$INSTALL_CMD" ] && ask_yes_no "Installer gamemode maintenant ?"; then
            echo ">> Installation de gamemode"
            if ! $INSTALL_CMD; then
                echo "ERREUR : l'installation de gamemode a échoué." >&2
            fi
        else
            echo "Installation refusée." >&2
        fi
    fi

    # gamemode reste absent (refus, échec ou gestionnaire non reconnu) : on
    # demande si le simulateur doit tout de même être lancé sans lui.
    if ! command -v gamemoderun >/dev/null 2>&1; then
        if ! ask_yes_no "Lancer quand même ?"; then
            echo "Lancement annulé." >&2
            exit 1
        fi
    fi
fi

if command -v gamemoderun >/dev/null 2>&1; then
    echo ">> Lancement via gamemode"
    exec gamemoderun "$BIN"
else
    echo ">> Lancement normal"
    exec "$BIN"
fi
