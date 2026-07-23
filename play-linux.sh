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
