#!/usr/bin/env python3
"""
Nom du fichier : essences.py
Description : Quelle essence porte une nature de la BD TOPO ou de la BD Forêt.
Auteur : O. Booklage
Date : Août 2026
Licence : GPL v2
"""

from foret.reglages import (CONIFERE, FEUILLU, MIXTE, MOTS_CONIFERE,
                            NATURES_ARBOREES, PIN, RE_PIN)

def porte_des_arbres(nature):
    """La nature BD TOPO donnée porte-t-elle des arbres au sens du semis ?"""
    bas = (nature or "").lower()
    return any(mot in bas for mot in NATURES_ARBOREES)


def essence_bdtopo(nature):
    """Essence de repli déduite de la nature BD TOPO, là où la BD Forêt ne dit
       rien (haie, bosquet, bois sans inventaire). "Bois" ne précisant pas
       l'essence, on le traite en feuillu."""
    bas = (nature or "").lower()
    if "mixte" in bas:
        return MIXTE
    return CONIFERE if "conifère" in bas else FEUILLU


def essence_bdforet(tfv, essence):
    """Niveau d'essence pour une formation de la BD Forêt, ou 0 si ce n'en est
       pas une (lande, formation herbacée : elles ne colorent rien, le contour
       vient de la BD TOPO)."""
    if not tfv.startswith("Forêt") and not tfv.startswith("Peupleraie"):
        return 0
    bas = essence.lower()
    if "mixte" in bas:
        return MIXTE
    if RE_PIN.search(bas):
        return PIN
    return CONIFERE if any(mot in bas for mot in MOTS_CONIFERE) else FEUILLU
