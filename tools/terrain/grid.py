"""
grid.py
Conversions entre grille (colonnes/rangées), coordonnées monde (mètres,
centrées sur 0) et lon/lat, pour un terrain de dimensions données. Mêmes
formules que render::Terrain (calage en repère monde), partagées par les
outils qui recadrent ou recalent un terrain (crop_zombie_map.py) ; auparavant
des fonctions imbriquées (closures) dans le main() de ce script.

Auteur : O. Booklage
Licence : GPL v2
"""


class Grid:
    """Conversions grille <-> monde <-> lon/lat pour un terrain de cols x rows
       cellules, couvrant width_m x height_m mètres et l'emprise lon/lat
       donnée. Une instance par terrain (source, ou recadré)."""

    def __init__(self, cols, rows, width_m, height_m, lon_min, lon_max, lat_min, lat_max):
        self.cols = cols
        self.rows = rows
        self.width_m = width_m
        self.height_m = height_m
        self.lon_min = lon_min
        self.lon_max = lon_max
        self.lat_min = lat_min
        self.lat_max = lat_max
        self.half_w = 0.5 * width_m
        self.half_h = 0.5 * height_m
        self.dx = width_m / (cols - 1)
        self.dz = height_m / (rows - 1)

    def col_of(self, x):
        """Colonne de grille (flottante) pour l'abscisse monde x (est)."""
        return (x + self.half_w) / self.width_m * (self.cols - 1)

    def row_of(self, z):
        """Rangée de grille (flottante) pour l'ordonnée monde z (sud)."""
        return (z + self.half_h) / self.height_m * (self.rows - 1)

    def x_at(self, i):
        """Abscisse monde de la colonne de grille i (0 = ouest)."""
        return -self.half_w + i * self.dx

    def z_at(self, j):
        """Ordonnée monde de la rangée de grille j (0 = nord)."""
        return -self.half_h + j * self.dz

    def lon_of(self, x):
        """Longitude correspondant à l'abscisse monde x."""
        return self.lon_min + (x / self.width_m + 0.5) * (self.lon_max - self.lon_min)

    def lat_of(self, z):
        """Latitude correspondant à l'ordonnée monde z."""
        return self.lat_max - (z / self.height_m + 0.5) * (self.lat_max - self.lat_min)
