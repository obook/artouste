#!/usr/bin/env bash
#
# scripts/notice.sh
# Compilation de la notice PDF (docs/notice.tex), extraite de build.sh.
# Sourcé par build.sh, qui appelle build_notice() quand l'option -d est
# donnée. Suppose le répertoire de travail à la racine du dépôt (comme
# build.sh, qui s'y place avant de sourcer ce fichier).
#
# Auteur : O. Booklage
# Licence : GPL v2

# Régénère docs/notice.pdf. Régénérée avant le packaging pour que l'archive
# embarque une version à jour. Le PDF est versionné (la CI de release ne
# compile pas le LaTeX) : après une modif de docs/notice.tex, lance
# ./build.sh -d puis commite le PDF régénéré. Quitte le script appelant
# (exit 1) si la compilation LaTeX échoue.
build_notice() {
    echo ">> Notice PDF (docs/notice.tex)"
    if ! command -v lualatex >/dev/null 2>&1; then
        echo "   lualatex introuvable : notice PDF non régénérée." >&2
        echo "   Installe TeX Live (paquet texlive-luatex) puis relance ./build.sh -d." >&2
        return 0
    fi

    # Chaîne de version affichée dans la notice : dernière release (tag v*,
    # sans le préfixe v) suivie du numéro de commit en décimal, c'est-à-dire
    # le nombre total de commits (compteur séquentiel), ex. 0.8.0.231. Sans
    # git ni tag, on écrit une valeur neutre. docs/version.tex n'est pas
    # versionné : il est régénéré à chaque ./build.sh -d. Le || true évite que
    # set -e n'interrompe le script quand git échoue (hors dépôt, sans tag).
    local rel commit version
    rel="$(git describe --tags --abbrev=0 --match 'v*' 2>/dev/null | sed 's/^v//' || true)"
    commit="$(git rev-list --count HEAD 2>/dev/null || true)"
    if [ -n "$rel" ] && [ -n "$commit" ]; then
        version="${rel}.${commit}"
    else
        version="version de développement"
    fi
    printf '%s\n' "\\newcommand{\\noticeversion}{${version}}" > docs/version.tex
    echo "   Version : $version"

    # Double passe pour stabiliser la mise en page, puis ménage des fichiers
    # intermédiaires. La sortie va dans un log, affichée seulement en cas d'échec.
    local doc_log="docs/notice.build.log"
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
}
