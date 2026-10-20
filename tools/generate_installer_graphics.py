#!/usr/bin/env python3
"""Génère les images BMP 24 bits pour l'installateur Inno Setup de TSA.
- wizard_banner.bmp : 164 x 314 px (fond bleu/cyan dégradé sombre d'ingénierie, logo et typographie TSA)
- wizard_small.bmp : 55 x 58 px (logo TSA compact sur fond neutre)
Aucune dépendance externe (génération binaire BMP pure).
"""
import struct
import math
import os

def create_bmp(width, height, pixel_func):
    """Crée un fichier BMP 24 bits non compressé."""
    # En BMP, chaque ligne doit avoir une taille multiple de 4 octets
    row_size = (width * 3 + 3) & ~3
    image_size = row_size * height
    file_size = 54 + image_size
    
    # En-tête du fichier (14 octets)
    file_header = struct.pack('<2sIHHI', b'BM', file_size, 0, 0, 54)
    # En-tête DIB BITMAPINFOHEADER (40 octets)
    dib_header = struct.pack('<IIIHHIIIIII', 40, width, height, 1, 24, 0, image_size, 2835, 2835, 0, 0)
    
    data = bytearray()
    # Les lignes de pixels s'écrivent de bas en haut
    for y in range(height):
        row = bytearray()
        for x in range(width):
            # Inverser y pour pixel_func (y=0 en haut)
            b, g, r = pixel_func(x, height - 1 - y)
            row.extend((int(b) & 0xFF, int(g) & 0xFF, int(r) & 0xFF))
        # Remplissage pour multiple de 4
        while len(row) % 4 != 0:
            row.append(0)
        data.extend(row)
        
    return file_header + dib_header + data

def banner_pixel(x, y):
    # Dimensions 164 x 314
    # Dégradé vertical bleu nuit (#060913) vers bleu marine profond (#0A172E)
    t = y / 314.0
    r = int(6 + t * (12 - 6))
    g = int(9 + t * (25 - 9))
    b = int(19 + t * (55 - 19))
    
    # Légère trame de grille d'ingénierie subtile
    if (x % 20 == 0 or y % 20 == 0) and y > 60:
        r = min(255, r + 10)
        g = min(255, g + 16)
        b = min(255, b + 28)
        
    # Bandeau / accentuation cyan (#00F2FE) et bleu (#0072FF)
    # Cercle ou symbole structural vers le haut (centre x=82, y=90)
    cx, cy = 82, 85
    dist = math.hypot(x - cx, y - cy)
    if dist < 42:
        # Anneau cyan lumineux
        if 36 <= dist <= 41:
            return 254, 242, 0 # B, G, R pour #00F2FE
        elif dist < 36:
            # Intérieur dégradé bleu royal
            factor = (36 - dist) / 36.0
            r = int(r * (1 - factor) + 0 * factor)
            g = int(g * (1 - factor) + 114 * factor)
            b = int(b * (1 - factor) + 255 * factor)
            
    # Lignes obliques de structure (treillis / ossature 3D stylisée)
    if 150 <= y <= 270 and abs((x * 1.5 - y) % 35) < 1.5:
        r = min(255, r + 15)
        g = min(255, g + 35)
        b = min(255, b + 60)
        
    # Ligne d'accent cyan verticale à droite
    if x >= 161:
        return 254, 242, 0 # cyan
        
    return b, g, r

def small_pixel(x, y):
    # Dimensions 55 x 58
    # Fond sombre propre #0A101D
    r, g, b = 10, 16, 29
    cx, cy = 27, 29
    dist = math.hypot(x - cx, y - cy)
    if 18 <= dist <= 23:
        return 254, 242, 0 # Cyan #00F2FE
    elif dist < 18:
        # Coeur bleu TSA #0072FF
        return 255, 114, 0
    return b, g, r

if __name__ == '__main__':
    out_dir = r"d:\Tsaraloha Platforme\TSA\packaging"
    banner_path = os.path.join(out_dir, "wizard_banner.bmp")
    small_path = os.path.join(out_dir, "wizard_small.bmp")
    
    with open(banner_path, "wb") as f:
        f.write(create_bmp(164, 314, banner_pixel))
    print(f"Créé : {banner_path} ({os.path.getsize(banner_path)} octets)")
    
    with open(small_path, "wb") as f:
        f.write(create_bmp(55, 58, small_pixel))
    print(f"Créé : {small_path} ({os.path.getsize(small_path)} octets)")
