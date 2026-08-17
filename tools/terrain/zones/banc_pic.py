"""
banc_pic
Banc d'essai : 3 x 3 km autour du sommet du Pic du Midi de Bigorre, grille
1536, soit une maille de ~2 m, la finesse native du LiDAR HD. La heightmap EST
alors le relevé laser : tout écart visible entre le sol et la fenêtre de relief
vient du moteur ou du drapage de la photo, jamais de la donnée. Carte d'étude,
pas de tourisme.
"""

ZONE = {
    "bbox": (0.1227, 0.1595, 42.9234, 42.9504),
    "recolor_sea": False,
    "start": (0.1347, 42.9292),  # replat du lac d'Oncet
    "start_heading": 31,         # face au sommet
    "height_min": 0.0,
    "grid": 1536,      # mailles ~2 m : la heightmap à la finesse du laser
    "ortho_px": 4096,  # ortho ~0,73 m/px sur 3 km
    "title": "Banc d'essai du Pic du Midi (3 km, heightmap au laser)",
    "landmarks": [
        ("Pic du Midi de Bigorre", 0.1411, 42.9369),
        ("Pic de Sencours", 0.150952, 42.926766),
        ("Lac d'Oncet", 0.1347, 42.9292),
    ],
}
