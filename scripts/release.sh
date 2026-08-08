#!/usr/bin/env bash
#
# scripts/release.sh
# Publie une version : pose le tag, le pousse (ce qui démarre la compilation
# des archives par la forge) et crée aussitôt la release avec ses notes.
#
# À lancer une fois les notes écrites dans docs/RELEASE_NOTES.md et la version
# bumpée dans CMakeLists.txt, commitées et poussées :
#
#     ./scripts/release.sh v1.2.3
#
# Pourquoi un script plutôt que trois commandes à la main : la release doit
# être créée par vous et non par la forge, dont le compte robot resterait
# inscrit comme auteur (l'auteur est figé à la création). Tant que c'était une
# étape séparée, elle a été oubliée trois fois (v0.25.0, v0.27.0, v0.30.2).
#
# Auteur : O. Booklage
# Licence : GPL v2

set -euo pipefail

cd "$(dirname "$0")/.."

NOTES=docs/RELEASE_NOTES.md

erreur() {
    echo "ERREUR : $*" >&2
    exit 1
}

tag="${1:-}"
[ -n "$tag" ] || erreur "usage : $0 vX.Y.Z"
[[ "$tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-.+)?$ ]] || erreur "tag mal formé : $tag (attendu vX.Y.Z)"

command -v gh >/dev/null 2>&1 || erreur "gh introuvable (interface en ligne de commande de GitHub)."
[ -f "$NOTES" ] || erreur "$NOTES introuvable."

# Un tag posé sur un arbre sale livrerait autre chose que ce qui est publié.
[ -z "$(git status --porcelain)" ] || erreur "des modifications ne sont pas commitées."

# La compilation se fait à partir du dépôt distant, pas du disque local : si la
# branche n'est pas poussée, les archives ne contiendraient pas les derniers
# commits.
branche="$(git rev-parse --abbrev-ref HEAD)"
git fetch --quiet origin "$branche"
[ "$(git rev-parse HEAD)" = "$(git rev-parse "origin/$branche")" ] \
    || erreur "la branche $branche n'est pas à jour avec origin (commitez et poussez d'abord)."

git rev-parse -q --verify "refs/tags/$tag" >/dev/null && erreur "le tag $tag existe déjà."
gh release view "$tag" >/dev/null 2>&1 && erreur "la release $tag existe déjà."

# La version en tête des notes doit être celle qu'on publie : sans ce contrôle,
# on publierait la version précédente, les notes n'ayant pas été écrites.
tete="$(awk '/^## /{print $2; exit}' "$NOTES")"
[ "$tete" = "$tag" ] || erreur "$NOTES commence par $tete, pas $tag (notes pas encore écrites ?)."

# Le fichier est un journal cumulatif, la version publiée en tête suivie de tout
# l'historique. On n'en prend que la première section, sinon la release recopie
# les versions précédentes et le lecteur ne sait plus ce qui est nouveau.
extrait="$(mktemp)"
trap 'rm -f "$extrait"' EXIT
awk '/^## /{n++} n==1' "$NOTES" > "$extrait"

echo ">> Notes de $tag"
sed 's/^/   /' "$extrait"
echo

echo ">> Tag et compilation des archives"
git tag -a "$tag" -m "$tag"
git push origin "$tag"

# Un tag portant un tiret (v1.2.3-rc1, v0.0.0-essai) est un essai ou une version
# candidate : publiée en pré-version, elle ne devient pas celle que propose le
# bouton de téléchargement du dépôt.
prerelease=()
case "$tag" in
    *-*) prerelease=(--prerelease) ;;
esac

echo ">> Release $tag"
gh release create "$tag" --title "$tag" --notes-file "$extrait" "${prerelease[@]}"

echo
echo "Release créée. La forge compile les archives (une quinzaine de minutes)"
echo "et les y attachera toute seule. Suivi :"
echo "    gh run watch \$(gh run list --workflow=release.yml --limit 1 --json databaseId -q '.[0].databaseId')"
