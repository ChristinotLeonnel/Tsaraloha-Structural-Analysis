#!/usr/bin/env python3
"""Vérifie un paquet distribuable TSA (cmake --build --preset ninja-release --target package_TSA).

Contrôles :
  1. Chaque .exe / .dll du paquet : toutes ses DLL importées (table d'import PE) sont fournies par le paquet
     (même dossier ou racine) ou par Windows. Le runtime MSVC (msvcp140, vcruntime140…) n'est PAS considéré
     comme fourni par Windows : absent d'un poste propre, il doit être dans le paquet.
  2. Fichiers indispensables : plugin de plateforme Qt, moteur d'icônes SVG, traduction, ressources OCCT,
     manifeste, référence des raccourcis ; moteur OpenSees et son init.tcl s'il est inclus.
  3. MANIFEST.json : chaque fichier listé existe et son SHA-256 correspond ; aucun fichier non listé.
  4. Aucune DLL Qt 5 ni bibliothèque inutile connue (VTK, Tcl/Tk, Doxygen).

Usage : python tools/verify_package.py <dossier du paquet>   (code de sortie 1 si erreur)
"""
import hashlib
import json
import os
import struct
import sys

# Bibliothèques redistribuables : jamais acceptées comme « fournies par Windows ».
REDIST_PREFIXES = ("msvcp140", "vcruntime140", "concrt140", "vccorlib140", "vcomp140", "libiomp5md")
FORBIDDEN_PREFIXES = ("qt5", "vtk", "tcl86", "tk86", "doxygen")


def pe_imports(path):
    """Noms des DLL importées (imports normaux et différés) d'un fichier PE 32/64 bits."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] != b"MZ":
        return []
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        return []
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    dd = opt + (112 if magic == 0x20B else 96)
    sections = []
    sec = opt + opt_size
    for i in range(nsec):
        vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", data, sec + i * 40 + 8)
        sections.append((vaddr, max(vsize, rsize), raddr))

    def rva2off(rva):
        for vaddr, size, raddr in sections:
            if vaddr <= rva < vaddr + size:
                return rva - vaddr + raddr
        return None

    def cstr(off):
        end = data.index(b"\0", off)
        return data[off:end].decode("ascii", "replace")

    names = []
    for index, stride, name_field in ((1, 20, 12), (13, 32, 4)):   # import, delay-import
        rva, size = struct.unpack_from("<II", data, dd + index * 8)
        off = rva2off(rva) if rva else None
        while off is not None and off + stride <= len(data):
            name_rva = struct.unpack_from("<I", data, off + name_field)[0]
            if name_rva == 0:
                break
            noff = rva2off(name_rva)
            if noff is None:
                break
            names.append(cstr(noff))
            off += stride
    return names


def system_provides(name, sysdirs):
    low = name.lower()
    if low.startswith(("api-ms-", "ext-ms-")):
        return True
    if low.startswith(REDIST_PREFIXES):
        return False
    return any(os.path.exists(os.path.join(d, name)) for d in sysdirs)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    root = os.path.abspath(sys.argv[1])
    windir = os.environ.get("WINDIR", r"C:\Windows")
    sysdirs = [os.path.join(windir, "System32"), windir]
    errors, warnings = [], []

    binaries = []
    for dirpath, _, files in os.walk(root):
        for f in files:
            if f.lower().endswith((".exe", ".dll")) and f.lower() != "vc_redist.x64.exe":
                binaries.append(os.path.join(dirpath, f))
    checked = 0
    for b in binaries:
        rel = os.path.relpath(b, root)
        low = os.path.basename(b).lower()
        if low.startswith(FORBIDDEN_PREFIXES):
            errors.append(f"bibliothèque inutile ou incompatible distribuée : {rel}")
        here = os.path.dirname(b)
        # Processus séparé (exécutable d'un sous-dossier : OpenSees, convertisseurs de modules) : le chargeur de
        # Windows cherche dans SON dossier, jamais dans celui de l'application. Les DLL chargées dans le processus
        # principal (plugins Qt, plugins/) trouvent en revanche celles du dossier de l'application.
        separate = os.path.normcase(here) != os.path.normcase(root) and any(
            f.lower().endswith(".exe") for f in os.listdir(here))
        for imp in pe_imports(b):
            if os.path.exists(os.path.join(here, imp)) or (not separate and os.path.exists(os.path.join(root, imp))):
                continue
            if system_provides(imp, sysdirs):
                continue
            errors.append(f"{rel} : DLL manquante « {imp} »")
        checked += 1

    required = ["platforms/qwindows.dll", "iconengines/qsvgicon.dll", "imageformats/qsvg.dll",
                "translations/qtbase_fr.qm", "resources/occt/Shaders", "resources/occt/StdResource",
                "MANIFEST.json", "docs/shortcut.txt", "licenses/THIRD_PARTY_LICENSES.md", "Extensions"]
    exe = [f for f in os.listdir(root) if f.lower() in ("tsa.exe", "tsalab.exe")]
    if not exe:
        errors.append("exécutable principal absent")
    for r in required:
        if not os.path.exists(os.path.join(root, r)):
            errors.append(f"fichier indispensable absent : {r}")

    manifest = {}
    try:
        with open(os.path.join(root, "MANIFEST.json"), encoding="utf-8") as f:
            manifest = json.load(f)
    except Exception as e:  # noqa: BLE001
        errors.append(f"MANIFEST.json illisible : {e}")
    if manifest.get("openSeesIncluded"):
        for r in ("engines/OpenSees/bin/OpenSees.exe", "engines/OpenSees/lib/tcl8.6/init.tcl"):
            if not os.path.exists(os.path.join(root, r)):
                errors.append(f"moteur OpenSees incomplet : {r}")
    listed = set()
    for e in manifest.get("entries", []):
        p = os.path.join(root, e["path"])
        listed.add(os.path.normcase(os.path.normpath(p)))
        if not os.path.exists(p):
            errors.append(f"manifeste : fichier absent {e['path']}")
            continue
        h = hashlib.sha256(open(p, "rb").read()).hexdigest()
        if h.lower() != e["sha256"].lower():
            errors.append(f"manifeste : empreinte différente {e['path']}")
    for dirpath, _, files in os.walk(root):
        for f in files:
            p = os.path.normcase(os.path.normpath(os.path.join(dirpath, f)))
            if f != "MANIFEST.json" and p not in listed:
                warnings.append(f"fichier non listé au manifeste : {os.path.relpath(p, root)}")

    total = sum(os.path.getsize(os.path.join(d, f)) for d, _, fs in os.walk(root) for f in fs)
    for w in warnings:
        print("AVERT. :", w)
    for e in errors:
        print("ERREUR :", e)
    print(f"{checked} binaires analysés, {len(manifest.get('entries', []))} fichiers au manifeste, "
          f"{total / 1048576:.1f} Mo — {'OK' if not errors else str(len(errors)) + ' erreur(s)'}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
