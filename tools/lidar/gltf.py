"""
gltf.py
Écriture et relecture d'un glTF 2.0 binaire d'un seul maillage texturé.

Le moteur ne compile que les importeurs AC3D et glTF (voir
cmake/Dependencies.cmake) : un OBJ ne se charge pas. Le glTF binaire pèse en
outre le quart d'un OBJ texte à géométrie égale.

Auteur : O. Booklage
Licence : GPL v2
"""

import json
import struct

import numpy as np


def ecrire_glb(chemin, positions, normales, uvs, indices, image, nom):
    """Écrit un glTF 2.0 binaire d'un seul maillage texturé. Assimp le lit avec
       l'importeur glTF, le seul compilé avec AC3D par le projet."""
    tampons = [positions.astype("<f4"), normales.astype("<f4"),
               uvs.astype("<f4"), indices.astype("<u4")]
    binaire, vues, decalage = bytearray(), [], 0
    for tampon in tampons:
        octets = tampon.tobytes()
        vues.append({"buffer": 0, "byteOffset": decalage, "byteLength": len(octets)})
        binaire.extend(octets)
        # Chaque vue commence sur un multiple de quatre octets (exigence glTF).
        while len(binaire) % 4:
            binaire.append(0)
        decalage = len(binaire)

    scene = {
        "asset": {"version": "2.0", "generator": "artouste tools/observatoire.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": nom}],
        "meshes": [{"name": nom, "primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
            "indices": 3,
            "material": 0,
        }]}],
        "materials": [{
            "name": nom,
            "pbrMetallicRoughness": {
                "baseColorTexture": {"index": 0},
                "metallicFactor": 0.0,
                "roughnessFactor": 1.0,
            },
        }],
        "textures": [{"source": 0, "sampler": 0}],
        "images": [{"uri": image}],
        # Répétition horizontale : la bande de façade se répète le long des murs.
        # Verticalement on borne, pour ne pas ramener l'orthophoto sur la bande.
        "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 33071}],
        "buffers": [{"byteLength": len(binaire)}],
        "bufferViews": vues,
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions), "type": "VEC3",
             "min": positions.min(axis=0).tolist(), "max": positions.max(axis=0).tolist()},
            {"bufferView": 1, "componentType": 5126, "count": len(normales), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": len(uvs), "type": "VEC2"},
            {"bufferView": 3, "componentType": 5125, "count": len(indices), "type": "SCALAR"},
        ],
    }

    entete = json.dumps(scene, separators=(",", ":")).encode("utf-8")
    entete += b" " * (-len(entete) % 4)      # bourrage par des espaces
    corps = bytes(binaire)
    corps += b"\x00" * (-len(corps) % 4)     # bourrage par des zéros
    total = 12 + 8 + len(entete) + 8 + len(corps)
    with open(chemin, "wb") as sortie:
        sortie.write(struct.pack("<4sII", b"glTF", 2, total))
        sortie.write(struct.pack("<II", len(entete), 0x4E4F534A))   # "JSON"
        sortie.write(entete)
        sortie.write(struct.pack("<II", len(corps), 0x004E4942))    # "BIN"
        sortie.write(corps)


def relire_glb(chemin):
    """Relit l'en-tête d'un .glb écrit ici : renvoie (scène JSON, longueur du
       binaire). Sert au contrôle, sans dépendre d'une bibliothèque glTF."""
    donnees = chemin.read_bytes()
    magie, version, total = struct.unpack_from("<4sII", donnees, 0)
    if magie != b"glTF" or version != 2:
        raise RuntimeError(f"{chemin} : ce n'est pas un glTF binaire version 2")
    if total != len(donnees):
        raise RuntimeError(f"{chemin} : longueur annoncée {total}, fichier {len(donnees)}")
    longueur, sorte = struct.unpack_from("<II", donnees, 12)
    if sorte != 0x4E4F534A:
        raise RuntimeError(f"{chemin} : premier bloc non JSON")
    scene = json.loads(donnees[20:20 + longueur])
    longueur_bin, _ = struct.unpack_from("<II", donnees, 20 + longueur)
    return scene, longueur_bin
