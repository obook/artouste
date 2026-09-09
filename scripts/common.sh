#!/usr/bin/env bash
#
# scripts/common.sh
# Fonctions partagées par build.sh et play-linux.sh : question oui/non et
# détection/préfixage du gestionnaire de paquets système (apt, dnf, pacman,
# ou brew sous macOS).
# Ce fichier est sourcé (jamais exécuté directement) depuis la racine du
# dépôt ou depuis scripts/, peu importe : il ne contient que des fonctions.
#
# Auteur : O. Booklage
# Licence : GPL v2

# Pose une question oui/non. Renvoie 0 (oui) ou 1 (non). Avec ASSUME_YES=1
# (variable définie par le script appelant), répond toujours oui. Sans
# terminal interactif et sans ASSUME_YES, répond non par défaut : aucune
# action lourde n'est déclenchée à l'aveugle (CI, pipe, cron).
ask_yes_no() {
    local rep
    if [ "${ASSUME_YES:-0}" -eq 1 ]; then
        return 0
    fi
    if [ ! -t 0 ]; then
        return 1
    fi
    read -rp "$1 [o/N] : " rep
    # Motifs insensibles à la casse écrits à la main : la conversion "${rep,,}"
    # est du bash 4, et macOS ne fournit que bash 3.2.
    case "$rep" in
        [oO] | [oO][uU][iI] | [yY] | [yY][eE][sS]) return 0 ;;
        *) return 1 ;;
    esac
}

# Détecte le gestionnaire de paquets système et l'affiche sur stdout ("apt",
# "dnf", "pacman" ou "brew") ; rien si aucun n'est reconnu. Homebrew passe en
# premier : une machine macOS n'a aucun des trois autres, et l'inverse est vrai
# aussi, l'ordre n'a donc d'importance que pour la lisibilité.
detect_pkg_mgr() {
    if command -v brew >/dev/null 2>&1; then
        echo "brew"
    elif command -v apt-get >/dev/null 2>&1; then
        echo "apt"
    elif command -v dnf >/dev/null 2>&1; then
        echo "dnf"
    elif command -v pacman >/dev/null 2>&1; then
        echo "pacman"
    fi
}

# Préfixe la commande donnée ($1) par sudo si l'utilisateur courant n'est pas
# root -- ne jamais l'appeler pour brew, qui refuse de tourner en root.
# Affiche la commande (préfixée ou non) sur stdout dans tous les cas ;
# renvoie 1 si sudo est requis mais introuvable (la commande affichée reste
# alors non préfixée, inutilisable telle quelle) : à l'appelant de décider
# comment réagir, les deux scripts ne se comportent pas pareil sur ce cas.
prefix_sudo() {
    local cmd="$1"
    if [ "$(id -u)" -eq 0 ]; then
        echo "$cmd"
        return 0
    fi
    if command -v sudo >/dev/null 2>&1; then
        echo "sudo $cmd"
        return 0
    fi
    echo "$cmd"
    return 1
}
