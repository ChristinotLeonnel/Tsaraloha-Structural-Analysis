#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
TSALib Tool - Outil CLI de création, validation, packaging et gestion de bibliothèques TSALib.
Tsaraloha Structural Analysis (TSA)

Commandes disponibles :
  - init / scaffold  : Créer l'arborescence et les modèles d'une nouvelle extension
  - validate         : Valider la conformité d'une extension selon les normes TSA
  - pack             : Empaqueter une extension au format autonome .tsalib
  - unpack           : Extraire et vérifier l'intégrité d'un package .tsalib
  - inspect          : Inspecter les métadonnées d'un package sans extraction
  - csv-to-sections  : Importer un catalogue de sections depuis un fichier CSV
  - csv-to-materials : Importer des matériaux depuis un fichier CSV
"""

import sys
import os
import re
import json
import csv
import struct
import zlib
import hashlib
import argparse
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple

PACKAGE_MAGIC = 0x000142494C415354
FORMAT_VERSION = 1

VALID_UNITS = {
    "pa", "kpa", "mpa", "gpa", "bar", "n/mm2",
    "kg/m3", "kg/m^3", "t/m3", "g/cm3",
    "n", "kn", "mn",
    "m", "dm", "cm", "mm",
    "m2", "cm2", "mm2",
    "m3", "cm3", "mm3",
    "m4", "cm4", "mm4",
    "deg", "°", "rad",
    "1/k", "k^-1"
}

def is_valid_id(ext_id: str) -> bool:
    if not ext_id or len(ext_id) > 128:
        return False
    return bool(re.match(r"^[a-z0-9_-]+(\.[a-z0-9_-]+)*$", ext_id))

def is_safe_relative_path(rel_path: str) -> bool:
    if not rel_path or not rel_path.strip():
        return False
    p = rel_path.replace("\\", "/")
    if p.startswith("/") or ":" in p:
        return False
    parts = [part for part in p.split("/") if part]
    for part in parts:
        if part in ("..", "."):
            return False
    return True

# -----------------------------------------------------------------------------
# QDataStream Helpers (Qt 6, LittleEndian)
# -----------------------------------------------------------------------------

def qbytearray_pack(data: bytes) -> bytes:
    return struct.pack('<I', len(data)) + data

def qbytearray_unpack(stream_bytes: bytes, offset: int) -> Tuple[bytes, int]:
    length = struct.unpack_from('<I', stream_bytes, offset)[0]
    offset += 4
    if length == 0xFFFFFFFF:
        return b'', offset
    data = stream_bytes[offset:offset+length]
    offset += length
    return data, offset

def qstring_pack(s: str) -> bytes:
    encoded = s.encode('utf-16le')
    return struct.pack('<I', len(encoded)) + encoded

def qstring_unpack(stream_bytes: bytes, offset: int) -> Tuple[str, int]:
    byte_len = struct.unpack_from('<I', stream_bytes, offset)[0]
    offset += 4
    if byte_len in (0xFFFFFFFF, 0):
        return '', offset
    s = stream_bytes[offset:offset+byte_len].decode('utf-16le')
    offset += byte_len
    return s, offset

def qcompress(data: bytes, level: int = 9) -> bytes:
    comp = zlib.compress(data, level)
    return struct.pack('>I', len(data)) + comp

def quncompress(compressed_data: bytes) -> bytes:
    if len(compressed_data) < 4:
        raise ValueError("Payload de compression invalide")
    orig_len = struct.unpack('>I', compressed_data[:4])[0]
    raw = zlib.decompress(compressed_data[4:])
    if len(raw) != orig_len:
        raise ValueError(f"Taille décompressée attendue {orig_len}, obtenue {len(raw)}")
    return raw

# -----------------------------------------------------------------------------
# Commandes
# -----------------------------------------------------------------------------

def cmd_init(args):
    ext_id = args.id.strip().lower()
    if not is_valid_id(ext_id):
        print(f"[ERREUR] Identifiant invalide : '{ext_id}'. Format : minuscules, chiffres, points, tirets.", file=sys.stderr)
        sys.exit(1)

    target_dir = Path(args.target) if args.target else Path("Extensions") / ext_id
    target_dir.mkdir(parents=True, exist_ok=True)

    categories = [cat.strip().lower() for cat in args.categories.split(",") if cat.strip()]

    manifest = {
        "$schema": "https://tsaraloha.org/schemas/tsalib-manifest-v1.json",
        "id": ext_id,
        "name": args.name or f"Bibliothèque {ext_id}",
        "version": args.version or "1.0.0",
        "format_version": "1.0",
        "minimum_tsa_version": "0.1.0",
        "author": args.author or "Ingénieur TSA",
        "license": args.license or "MIT",
        "description": args.description or "Bibliothèque de composants structuraux pour TSA.",
        "kind": "data",
        "categories": categories
    }

    manifest_path = target_dir / "manifest.json"
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=4, ensure_ascii=False)
    print(f"[OK] Manifeste créé : {manifest_path}")

    # Sous-répertoires
    if "materials" in categories:
        mat_dir = target_dir / "Materials"
        mat_dir.mkdir(exist_ok=True)
        sample_mat = {
            "id": f"{ext_id.replace('.', '_')}_c25",
            "name": "Béton C25/30 Modèle",
            "category": "Concrete",
            "version": "1.0.0",
            "standard": {
                "name": "EN 1992-1-1",
                "edition": "2004",
                "clause": "Tableau 3.1",
                "source": "CEN"
            },
            "mechanical": {
                "density": { "value": 2500.0, "unit": "kg/m3" },
                "young_modulus": { "value": 31000.0, "unit": "MPa" },
                "poisson_ratio": 0.20,
                "thermal_coeff": { "value": 1.0e-5, "unit": "1/K" }
            },
            "strength": {
                "fck": { "value": 25.0, "unit": "MPa" }
            },
            "visual": {
                "base_color": "#7A8288",
                "roughness": 0.85,
                "metallic": 0.05,
                "textures": { "albedo": "Textures/concrete.png" }
            }
        }
        with open(mat_dir / "sample_concrete_c25.json", "w", encoding="utf-8") as f:
            json.dump(sample_mat, f, indent=4, ensure_ascii=False)
        print(f"[OK] Fiche matériau exemple : {mat_dir / 'sample_concrete_c25.json'}")

    if "sections" in categories:
        sec_dir = target_dir / "Sections"
        sec_dir.mkdir(exist_ok=True)
        sample_sec = {
            "id": "rect_300x500",
            "name": "Section Rectangulaire 300x500",
            "category": "Concrete",
            "shape_type": "Rectangular",
            "version": "1.0.0",
            "dimensions": {
                "width": 0.30,
                "height": 0.50
            },
            "properties": {
                "area": 0.15,
                "ix": 0.003125,
                "iy": 0.001125,
                "wx": 0.0125,
                "wy": 0.0075
            }
        }
        with open(sec_dir / "rect_300x500.json", "w", encoding="utf-8") as f:
            json.dump(sample_sec, f, indent=4, ensure_ascii=False)
        print(f"[OK] Fiche section exemple : {sec_dir / 'rect_300x500.json'}")

    if "profiles" in categories:
        prof_dir = target_dir / "Profiles"
        prof_dir.mkdir(exist_ok=True)
        sample_prof = {
            "id": "ipe200",
            "name": "IPE 200",
            "category": "Steel",
            "shape_type": "IShape",
            "version": "1.0.0",
            "standard": {
                "name": "EN 10365",
                "edition": "2017",
                "clause": "Table Standard",
                "source": "CEN"
            },
            "dimensions": {
                "width": 0.100,
                "height": 0.200,
                "tw": 0.0056,
                "tf": 0.0085,
                "r": 0.012
            },
            "properties": {
                "area": 0.00285,
                "ix": 1.94e-5,
                "iy": 1.42e-6,
                "wx": 1.94e-4,
                "wy": 2.85e-5
            }
        }
        with open(prof_dir / "ipe200.json", "w", encoding="utf-8") as f:
            json.dump(sample_prof, f, indent=4, ensure_ascii=False)
        print(f"[OK] Fiche profilé exemple : {prof_dir / 'ipe200.json'}")

    if "textures" in categories:
        tex_dir = target_dir / "Textures"
        tex_dir.mkdir(exist_ok=True)
        tex_idx = {
            "version": "1.0",
            "textures": []
        }
        with open(tex_dir / "textures.json", "w", encoding="utf-8") as f:
            json.dump(tex_idx, f, indent=4, ensure_ascii=False)
        print(f"[OK] Registre de textures : {tex_dir / 'textures.json'}")

    if "standards" in categories:
        std_dir = target_dir / "Standards"
        std_dir.mkdir(exist_ok=True)
        std_obj = {
            "id": "EN1990",
            "name": "Eurocode - Bases de calcul des structures",
            "edition": "2002",
            "scope": "Principes de calcul et de sécurité"
        }
        with open(std_dir / "EN1990.json", "w", encoding="utf-8") as f:
            json.dump(std_obj, f, indent=4, ensure_ascii=False)
        print(f"[OK] Fiche norme exemple : {std_dir / 'EN1990.json'}")

    # README.md
    readme_path = target_dir / "README.md"
    readme_content = f"""# {args.name or ext_id}

- **ID :** `{ext_id}`
- **Version :** `{args.version or '1.0.0'}`
- **Auteur :** {args.author or 'Non spécifié'}
- **Licence :** {args.license or 'MIT'}

## Description
{args.description or 'Bibliothèque TSALib pour TSA.'}

## Utilisation
1. Ajoutez vos fichiers JSON dans les dossiers appropriés (`Materials/`, `Sections/`, etc.).
2. Dans TSA, ouvrez **Structure & Sections** > **Gestionnaire TSALib...**.
3. Cliquez sur **Recharger tout** pour synchroniser sans redémarrage.
"""
    with open(readme_path, "w", encoding="utf-8") as f:
        f.write(readme_content)

    print(f"\n[SUCCÈS] Bibliothèque '{ext_id}' créée avec succès dans : {target_dir}")

def cmd_validate(args):
    target_dir = Path(args.dir)
    if not target_dir.exists() or not target_dir.is_dir():
        print(f"[ERREUR] Répertoire introuvable : {target_dir}", file=sys.stderr)
        sys.exit(1)

    manifest_path = target_dir / "manifest.json"
    if not manifest_path.exists():
        print(f"[ERREUR] Fichier 'manifest.json' absent de : {target_dir}", file=sys.stderr)
        sys.exit(1)

    errors = []
    warnings = []

    # Validation manifest
    try:
        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)
        if not is_valid_id(manifest.get("id", "")):
            errors.append(f"manifest.json: 'id' invalide : '{manifest.get('id')}'.")
        if not manifest.get("name"):
            errors.append("manifest.json: 'name' manquant ou vide.")
        if not manifest.get("version"):
            warnings.append("manifest.json: 'version' manquant.")
        if not manifest.get("categories"):
            warnings.append("manifest.json: aucune catégorie déclarée.")
    except Exception as e:
        errors.append(f"manifest.json: Erreur de parsing JSON : {e}")

    # Validation des fichiers de matériaux
    materials_dir = target_dir / "Materials"
    if materials_dir.exists():
        for mf in materials_dir.glob("*.json"):
            try:
                with open(mf, "r", encoding="utf-8") as f:
                    data = json.load(f)
                mid = data.get("id", "")
                if not is_valid_id(mid):
                    errors.append(f"{mf.name}: id invalide : '{mid}'")
                if not data.get("name"):
                    errors.append(f"{mf.name}: nom vide")

                # Mécanique
                mech = data.get("mechanical", {})
                e_mod = mech.get("young_modulus", {})
                if isinstance(e_mod, dict):
                    unit = e_mod.get("unit", "").lower()
                    if unit and unit not in VALID_UNITS:
                        errors.append(f"{mf.name}: unité de module non supportée : '{unit}'")
                    if e_mod.get("value", 0) <= 0:
                        warnings.append(f"{mf.name}: module d'Young nul ou négatif")

                rho = mech.get("density", {})
                if isinstance(rho, dict):
                    unit = rho.get("unit", "").lower()
                    if unit and unit not in VALID_UNITS:
                        errors.append(f"{mf.name}: unité de densité non supportée : '{unit}'")
                    if rho.get("value", 0) <= 0:
                        warnings.append(f"{mf.name}: densité nulle ou négative")

                nu = mech.get("poisson_ratio", 0.2)
                if nu < -1.0 or nu >= 0.5:
                    errors.append(f"{mf.name}: coefficient de Poisson aberrant : {nu} (attendu dans [-1.0, 0.5[)")

            except Exception as e:
                errors.append(f"{mf.name}: JSON corrompu : {e}")

    # Validation des sections
    for sec_sub in ["Sections", "Profiles"]:
        sec_dir = target_dir / sec_sub
        if sec_dir.exists():
            for sf in sec_dir.glob("*.json"):
                try:
                    with open(sf, "r", encoding="utf-8") as f:
                        data = json.load(f)
                    sid = data.get("id", "")
                    if not is_valid_id(sid):
                        errors.append(f"{sf.name}: id de section invalide : '{sid}'")
                    dims = data.get("dimensions", {})
                    w = dims.get("width", 0.0)
                    h = dims.get("height", 0.0)
                    d = dims.get("diameter", 0.0)
                    if w <= 0.0 and h <= 0.0 and d <= 0.0:
                        errors.append(f"{sf.name}: dimensions de section nulles ou négatives")
                except Exception as e:
                    errors.append(f"{sf.name}: JSON corrompu : {e}")

    print(f"\n--- Rapport d'Audit Normatif pour '{target_dir.name}' ---")
    if warnings:
        print(f"\n[AVERTISSEMENTS ({len(warnings)})] :")
        for w in warnings:
            print(f"  [!] {w}")
    if errors:
        print(f"\n[ERREURS ({len(errors)})] :")
        for err in errors:
            print(f"  [X] {err}")
        print("\n[ÉCHEC] L'extension présente des non-conformités.", file=sys.stderr)
        sys.exit(1)
    else:
        print(f"\n[SUCCÈS] Intégrité 100% validée sans aucune erreur !")

def cmd_pack(args):
    src_dir = Path(args.dir)
    if not src_dir.exists() or not src_dir.is_dir():
        print(f"[ERREUR] Dossier source introuvable : {src_dir}", file=sys.stderr)
        sys.exit(1)

    manifest_path = src_dir / "manifest.json"
    if not manifest_path.exists():
        print(f"[ERREUR] manifest.json absent : {manifest_path}", file=sys.stderr)
        sys.exit(1)

    with open(manifest_path, "rb") as f:
        manifest_raw = f.read()

    try:
        manifest_obj = json.loads(manifest_raw.decode("utf-8"))
        ext_id = manifest_obj.get("id", src_dir.name)
    except Exception as e:
        print(f"[ERREUR] manifest.json JSON invalide : {e}", file=sys.stderr)
        sys.exit(1)

    out_path = Path(args.output) if args.output else src_dir.parent / f"{ext_id}.tsalib"
    out_path.parent.mkdir(parents=True, exist_ok=True)

    files_to_pack = []
    for root, _, files in os.walk(src_dir):
        for f in files:
            full_p = Path(root) / f
            rel_p = full_p.relative_to(src_dir).as_posix()
            if rel_p.endswith(".tsalib") or rel_p.startswith("."):
                continue
            with open(full_p, "rb") as fp:
                data = fp.read()
            uncomp_size = len(data)
            sha256 = hashlib.sha256(data).digest()
            comp_data = qcompress(data, level=9)
            files_to_pack.append({
                "rel_path": rel_p,
                "uncompressed_size": uncomp_size,
                "sha256": sha256,
                "compressed_data": comp_data
            })

    # Écriture du flux binaire LittleEndian
    payload = bytearray()
    payload += struct.pack('<Q', PACKAGE_MAGIC)
    payload += struct.pack('<I', FORMAT_VERSION)
    payload += struct.pack('<I', 0) # flags

    payload += qbytearray_pack(manifest_raw)
    payload += struct.pack('<I', len(files_to_pack))

    for item in files_to_pack:
        payload += qstring_pack(item["rel_path"])
        payload += struct.pack('<Q', item["uncompressed_size"])
        payload += qbytearray_pack(item["sha256"])
        payload += qbytearray_pack(item["compressed_data"])

    with open(out_path, "wb") as f:
        f.write(payload)

    pkg_sha = hashlib.sha256(payload).hexdigest()
    print(f"[SUCCÈS] Package créé : {out_path}")
    print(f"  - Fichiers intégrés : {len(files_to_pack)}")
    print(f"  - Taille binaire     : {len(payload)} octets")
    print(f"  - Empreinte SHA-256  : {pkg_sha}")

def cmd_inspect(args):
    pkg_path = Path(args.package)
    if not pkg_path.exists():
        print(f"[ERREUR] Fichier introuvable : {pkg_path}", file=sys.stderr)
        sys.exit(1)

    with open(pkg_path, "rb") as f:
        data = f.read()

    offset = 0
    magic, ver, flags = struct.unpack_from('<QII', data, offset)
    offset += 16

    if magic != PACKAGE_MAGIC:
        print("[ERREUR] Magic bytes invalides (ce fichier n'est pas un package .tsalib valide).", file=sys.stderr)
        sys.exit(1)

    manifest_bytes, offset = qbytearray_unpack(data, offset)
    manifest = json.loads(manifest_bytes.decode('utf-8'))

    count = struct.unpack_from('<I', data, offset)[0]
    offset += 4

    files = []
    total_uncompressed = 0
    for _ in range(count):
        rel_p, offset = qstring_unpack(data, offset)
        uncomp_sz = struct.unpack_from('<Q', data, offset)[0]
        offset += 8
        sha256, offset = qbytearray_unpack(data, offset)
        comp_data, offset = qbytearray_unpack(data, offset)
        files.append((rel_p, uncomp_sz, len(comp_data), sha256.hex()))
        total_uncompressed += uncomp_sz

    print(f"\n--- Inspection du Package TSALib : {pkg_path.name} ---")
    print(f"  - Version Format    : {ver}")
    print(f"  - Identifiant ID    : {manifest.get('id')}")
    print(f"  - Nom Bibliothèque  : {manifest.get('name')}")
    print(f"  - Version Extension : {manifest.get('version')}")
    print(f"  - Auteur            : {manifest.get('author')}")
    print(f"  - Nombre de fichiers: {len(files)}")
    print(f"  - Taille décompressée : {total_uncompressed / 1024:.2f} Ko")
    print(f"  - Taille archive    : {len(data) / 1024:.2f} Ko\n")
    print(f"{'Fichier':<40} {'Décompressé':<15} {'Compressé':<15}")
    print("-" * 72)
    for p, u, c, _ in files:
        print(f"{p:<40} {u:<15} {c:<15}")

def cmd_unpack(args):
    pkg_path = Path(args.package)
    if not pkg_path.exists():
        print(f"[ERREUR] Fichier package introuvable : {pkg_path}", file=sys.stderr)
        sys.exit(1)

    with open(pkg_path, "rb") as f:
        data = f.read()

    offset = 0
    magic, ver, _ = struct.unpack_from('<QII', data, offset)
    offset += 16
    if magic != PACKAGE_MAGIC:
        print("[ERREUR] Signature magic invalide.", file=sys.stderr)
        sys.exit(1)

    manifest_bytes, offset = qbytearray_unpack(data, offset)
    manifest = json.loads(manifest_bytes.decode('utf-8'))
    ext_id = manifest.get('id', 'extracted_extension')

    dest_dir = Path(args.dest) if args.dest else Path("Extensions") / ext_id
    dest_dir.mkdir(parents=True, exist_ok=True)

    count = struct.unpack_from('<I', data, offset)[0]
    offset += 4

    manifest_found = False
    for _ in range(count):
        rel_p, offset = qstring_unpack(data, offset)
        uncomp_sz = struct.unpack_from('<Q', data, offset)[0]
        offset += 8
        sha256_bytes, offset = qbytearray_unpack(data, offset)
        comp_data, offset = qbytearray_unpack(data, offset)

        if not is_safe_relative_path(rel_p):
            print(f"[ERREUR SÉCURITÉ] Tentative de Path Traversal rejetée : {rel_p}", file=sys.stderr)
            sys.exit(1)

        raw = quncompress(comp_data)
        if len(raw) != uncomp_sz:
            print(f"[ERREUR] Taille incohérente pour {rel_p}", file=sys.stderr)
            sys.exit(1)
        if hashlib.sha256(raw).digest() != sha256_bytes:
            print(f"[ERREUR] Empreinte SHA-256 corrompue pour {rel_p}", file=sys.stderr)
            sys.exit(1)

        out_f = dest_dir / rel_p
        out_f.parent.mkdir(parents=True, exist_ok=True)
        with open(out_f, "wb") as fp:
            fp.write(raw)
        if rel_p.lower() == "manifest.json":
            manifest_found = True

    if not manifest_found:
        with open(dest_dir / "manifest.json", "wb") as fp:
            fp.write(manifest_bytes)

    print(f"[SUCCÈS] {count} fichiers extraits avec intégrité SHA-256 vérifiée dans : {dest_dir}")

def cmd_csv_to_sections(args):
    csv_path = Path(args.csv)
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    if not csv_path.exists():
        print(f"[ERREUR] Fichier CSV introuvable : {csv_path}", file=sys.stderr)
        sys.exit(1)

    count = 0
    with open(csv_path, "r", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        for row in reader:
            sec_id = row.get("id", "").strip().lower()
            if not sec_id:
                name_clean = row.get("name", "sec").strip().lower().replace(" ", "_")
                sec_id = re.sub(r'[^a-z0-9_]', '', name_clean)

            shape = row.get("shape_type", "Rectangular")
            w = float(row.get("width", 0.0))
            h = float(row.get("height", 0.0))
            tw = float(row.get("tw", 0.0))
            tf = float(row.get("tf", 0.0))
            area = float(row.get("area", 0.0))
            ix = float(row.get("ix", 0.0))
            iy = float(row.get("iy", 0.0))

            sec_json = {
                "id": sec_id,
                "name": row.get("name", sec_id),
                "category": row.get("category", "Steel" if shape == "IShape" else "Concrete"),
                "shape_type": shape,
                "version": "1.0.0",
                "dimensions": {
                    "width": w,
                    "height": h,
                    "tw": tw,
                    "tf": tf
                },
                "properties": {
                    "area": area,
                    "ix": ix,
                    "iy": iy
                }
            }
            out_file = out_dir / f"{sec_id}.json"
            with open(out_file, "w", encoding="utf-8") as out_fp:
                json.dump(sec_json, out_fp, indent=4, ensure_ascii=False)
            count += 1

    print(f"[SUCCÈS] {count} profilés/sections convertis depuis CSV dans : {out_dir}")

def main():
    parser = argparse.ArgumentParser(description="TSALib Tool - Outil de gestion des bibliothèques TSA")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # init
    p_init = subparsers.add_parser("init", aliases=["scaffold"], help="Créer une nouvelle extension TSALib")
    p_init.add_argument("id", help="Identifiant unique (ex: org.eurocode.timber)")
    p_init.add_argument("--name", help="Nom complet")
    p_init.add_argument("--version", default="1.0.0", help="Version sémantique initiale")
    p_init.add_argument("--author", default="TSA Engineering", help="Auteur")
    p_init.add_argument("--license", default="MIT", help="Licence")
    p_init.add_argument("--description", help="Description")
    p_init.add_argument("--target", help="Répertoire de destination")
    p_init.add_argument("--categories", default="materials,sections,profiles,textures,standards", help="Catégories séparées par des virgules")
    p_init.set_defaults(func=cmd_init)

    # validate
    p_val = subparsers.add_parser("validate", help="Valider la conformité d'une extension")
    p_val.add_argument("dir", help="Dossier de l'extension")
    p_val.set_defaults(func=cmd_validate)

    # pack
    p_pack = subparsers.add_parser("pack", help="Empaqueter en .tsalib")
    p_pack.add_argument("dir", help="Dossier de l'extension source")
    p_pack.add_argument("output", nargs="?", help="Chemin du fichier package de sortie (.tsalib)")
    p_pack.set_defaults(func=cmd_pack)

    # inspect
    p_insp = subparsers.add_parser("inspect", help="Inspecter un package .tsalib")
    p_insp.add_argument("package", help="Fichier .tsalib")
    p_insp.set_defaults(func=cmd_inspect)

    # unpack
    p_unpack = subparsers.add_parser("unpack", help="Extraire un package .tsalib")
    p_unpack.add_argument("package", help="Fichier .tsalib")
    p_unpack.add_argument("dest", nargs="?", help="Répertoire cible")
    p_unpack.set_defaults(func=cmd_unpack)

    # csv-to-sections
    p_csv = subparsers.add_parser("csv-to-sections", help="Convertir un CSV en fiches JSON de sections")
    p_csv.add_argument("csv", help="Fichier CSV source")
    p_csv.add_argument("out", help="Dossier de sortie pour les fiches JSON")
    p_csv.set_defaults(func=cmd_csv_to_sections)

    args = parser.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()
