#!/usr/bin/env python3
"""Lecture seule d'un .tsa : chunks, nombre d'éléments, rotules ; comparaison de la version HEAD et de la copie de travail."""
import glob, json, os, struct, subprocess, sys, zlib

repo = sys.argv[1]
HDR = 272


def chunks(raw):
    magic, major, minor = struct.unpack_from("<IHH", raw, 0)
    flags = struct.unpack_from("<I", raw, 20)[0]
    fsize = struct.unpack_from("<Q", raw, 32)[0]
    payload = raw[HDR:fsize]
    data = zlib.decompress(payload[4:]) if flags & 1 else payload
    out, off = {}, 0
    while off + 16 <= len(data):
        cid, size, count, _ = struct.unpack_from("<4sIII", data, off)
        off += 16
        out[cid.decode()] = (size, count, data[off:off + size])
        off += size
    return dict(version=f"{major}.{minor}", taille=len(raw), payload=len(payload), apercu=len(raw) - fsize, chunks=out)


def rstr(b, o):
    n = struct.unpack_from("<H", b, o)[0]
    return b[o + 2:o + 2 + n].decode("utf-8", "replace"), o + 2 + n


def bars(chunk):
    """Compte les barres ayant une rotule (structure lue dans TSAFileReader::readBarChunk)."""
    size, count, b = chunk
    o, hinged, n = 0, 0, 0
    ids = []
    for _ in range(count):
        bid = struct.unpack_from("<i", b, o)[0]; o += 4
        _, o = rstr(b, o)
        o += 8 + 1                      # start, end, role
        o += 4; _, o = rstr(b, o); o += 1 + 40    # section
        o += 4; _, o = rstr(b, o); o += 1 + 40    # matériau
        o += 8 + 1                      # rotation, excentrement
        rel = b[o:o + 12]; o += 12
        _, o = rstr(b, o)
        n += 1
        if any(rel):
            hinged += 1
            ids.append(bid)
    return n, hinged


def loads(raw_load):
    return None


print("=== examples/*.tsa : barres, rotules")
for path in sorted(glob.glob(os.path.join(repo, "examples", "*.tsa"))):
    c = chunks(open(path, "rb").read())
    ch = c["chunks"]
    try:
        nb, hi = bars(ch["BARS"]) if "BARS" in ch else (0, 0)
    except Exception as e:
        nb, hi = -1, repr(e)
    print(f"{os.path.basename(path):44} v{c['version']:5} barres {nb:5} rotules {hi}  poteaux {ch.get('COLS', (0, 0))[1]:5} câbles {ch.get('CABL', (0, 0))[1]:4} treillis {ch.get('TRUS', (0, 0))[1]:4}")

print("\n=== modifications locales (HEAD → copie de travail)")
for name in ("examples/TSA_Projet_Batiment_Complexe_L.tsa", "examples/TSA_Projet_Pont_Haubans_80-180-80m.tsa"):
    head = subprocess.run(["git", "-C", repo, "show", "HEAD:" + name], stdout=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW).stdout
    work = open(os.path.join(repo, name), "rb").read()
    a, b = chunks(head), chunks(work)
    print(name, f"version {a['version']} → {b['version']}, fichier {a['taille']} → {b['taille']} o, payload {a['payload']} → {b['payload']}, aperçu {a['apercu']} → {b['apercu']}")
    for k in sorted(set(a["chunks"]) | set(b["chunks"])):
        x, y = a["chunks"].get(k), b["chunks"].get(k)
        if x is None or y is None or x[2] != y[2]:
            print(f"   {k}: {(x[0], x[1]) if x else None} → {(y[0], y[1]) if y else None}")
