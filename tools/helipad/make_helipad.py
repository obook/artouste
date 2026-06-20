#!/usr/bin/env python3
"""
make_helipad.py
Construit l'helipad sous Blender (sans interface) et l'exporte au format AC3D
(.ac), comme le reste des modeles du projet : un disque plat texture, dans le
plan XY de Blender (donc a plat, normale vers le haut, une fois exporte).

Prerequis : le greffon AC3D pour Blender (celui d'Emmanuel) doit etre installe,
voir https://github.com/NikolaiVChr/Blender-AC3D (module "io_scene_ac3d").

A lancer ainsi (Blender utilise son propre Python, pas le venv du projet) :
    blender --background --python tools/helipad/make_helipad.py

La texture doit avoir ete generee au prealable (voir make_texture.py).

Auteur : O. Booklage
Licence : GPL v2
"""

import math
import os

import addon_utils
import bpy

RADIUS = 7.0   # rayon du disque, en metres (comme la version procedurale)
SEGMENTS = 96  # finesse du bord


def project_root():
    """Racine du depot, deduite de l'emplacement de ce script (tools/helipad/)."""
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.normpath(os.path.join(here, "..", ".."))


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    for block in (bpy.data.meshes, bpy.data.materials, bpy.data.images):
        for item in list(block):
            block.remove(item)


def build_disc():
    """Disque en eventail dans le plan XY, avec des UV qui inscrivent le disque
    dans la texture carree (centre de la texture = centre du disque)."""
    verts = [(0.0, 0.0, 0.0)]
    for i in range(SEGMENTS):
        a = (2.0 * math.pi * i) / SEGMENTS
        verts.append((RADIUS * math.cos(a), RADIUS * math.sin(a), 0.0))
    faces = []
    for i in range(1, SEGMENTS + 1):
        nxt = i + 1 if i < SEGMENTS else 1
        faces.append((0, i, nxt))

    mesh = bpy.data.meshes.new("helipad")
    mesh.from_pydata(verts, [], faces)
    mesh.update()

    uv = mesh.uv_layers.new(name="UVMap")
    for loop in mesh.loops:
        x, y, _ = verts[loop.vertex_index]
        uv.data[loop.index].uv = (0.5 + x / (2.0 * RADIUS), 0.5 + y / (2.0 * RADIUS))

    obj = bpy.data.objects.new("helipad", mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def make_material(texture_path):
    mat = bpy.data.materials.new("helipad")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(texture_path)
    links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    return mat


def main():
    root = project_root()
    out_dir = os.path.join(root, "assets", "models", "helipad")
    os.makedirs(out_dir, exist_ok=True)
    texture_path = os.path.join(out_dir, "helipad.png")
    out_ac = os.path.join(out_dir, "helipad.ac")

    addon_utils.enable("io_scene_ac3d", default_set=True, persistent=True)

    clear_scene()
    obj = build_disc()
    obj.data.materials.append(make_material(texture_path))

    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj

    # L'exportateur AC3D passe du repere Z-up de Blender au repere Y-up : le
    # disque se retrouve a plat dans le plan XZ, normale vers le haut.
    bpy.ops.export_scene.export_ac3d(filepath=out_ac)
    print("[helipad] exporte", out_ac)


if __name__ == "__main__":
    main()
