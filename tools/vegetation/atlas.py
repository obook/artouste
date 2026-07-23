"""
atlas.py
Assemblage de l'atlas de sprites d'arbres à partir de cellules déjà prêtes
(une par espèce) : réduction depuis un rendu supersamplé (finish(), utilisée
par make_trees_atlas.py) et collage côte à côte avec écriture du fichier final
(assemble_atlas(), partagée par make_trees_atlas.py et compose_trees_atlas.py,
qui ne diffèrent que par l'origine des cellules -- procédurale ou
photographique FlightGear).

Auteur : O. Booklage
Licence : GPL v2
"""

from PIL import Image, ImageFilter


def finish(img, size, blur):
    """Bords doux (léger flou, bon pour l'alpha-to-coverage) puis réduction
       vers une cellule carrée de size x size pixels."""
    img = img.filter(ImageFilter.GaussianBlur(blur))
    return img.resize((size, size), Image.LANCZOS)


def assemble_atlas(cells, out_path, verbe="écrit", note=""):
    """Colle les cellules (images RGBA de même taille) côte à côte dans l'ordre
       donné et enregistre l'atlas résultant vers out_path. verbe personnalise
       le message affiché ("écrit" ou "composé") ; note, si donné, y ajoute une
       précision (par exemple l'origine des sources)."""
    cell_w, cell_h = cells[0].size
    atlas = Image.new("RGBA", (cell_w * len(cells), cell_h), (0, 0, 0, 0))
    for i, cell in enumerate(cells):
        atlas.paste(cell, (i * cell_w, 0))
    atlas.save(out_path)
    suffix = f", {note}" if note else ""
    print(f"[vegetation] atlas {verbe} : {out_path} "
          f"({atlas.width}x{atlas.height}, {len(cells)} espèces{suffix})")
    return atlas
