"""
meta.py
Lecture et mise à jour de terrain.txt (clé valeur, une par ligne, commentaires
préfixés par #), mutualisées entre crop_zombie_map.py et refresh_ortho.py qui
en avaient chacun leur propre implémentation divergente.

Auteur : O. Booklage
Licence : GPL v2
"""


def read_meta(path):
    """Lit un fichier clé-valeur (terrain.txt) en dictionnaire str -> str, en
       ignorant les lignes vides et les commentaires (#)."""
    meta = {}
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 1)
        if len(parts) == 2:
            meta[parts[0]] = parts[1].strip()
    return meta


def update_keys(path, updates):
    """Réécrit terrain.txt en remplaçant la valeur des clés données (dict str
       -> valeur), le reste du fichier (commentaires, clés absentes de
       updates) inchangé."""
    lines = path.read_text().splitlines()
    out = []
    for line in lines:
        key = line.split(" ", 1)[0]
        out.append(f"{key} {updates[key]}" if key in updates else line)
    path.write_text("\n".join(out) + "\n")
