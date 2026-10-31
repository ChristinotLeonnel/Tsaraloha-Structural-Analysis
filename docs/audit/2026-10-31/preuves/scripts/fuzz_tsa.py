#!/usr/bin/env python3
"""Fuzzing du chargement .tsa (audit, aucune écriture dans le dépôt).

Chaque mutation agit APRÈS décompression sur les chunks, puis le fichier est recompressé et son CRC recalculé : le
contrôle d'intégrité ne protège pas contre un fichier fabriqué, seule la robustesse du lecteur est testée. Les
exécutables sont des copies placées dans le dossier de travail (un plantage ne verrouille pas ceux du build).

Usage : fuzz_tsa.py <semence.tsa> <dossier travail> <bin build> <nb itérations> [graine]
"""
import ctypes, json, os, random, shutil, struct, subprocess, sys, time, zlib, collections

SEM_FAILCRITICALERRORS, SEM_NOGPFAULTERRORBOX, SEM_NOOPENFILEERRORBOX = 0x1, 0x2, 0x8000
ctypes.windll.kernel32.SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX)

seed_file, work, build, iterations = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
graine = int(sys.argv[5]) if len(sys.argv) > 5 else 20261031
rng = random.Random(graine)
os.makedirs(work, exist_ok=True)
bins = os.path.join(work, "bin")
os.makedirs(bins, exist_ok=True)
for exe in ("tsa-ifc.exe", "tsa-mesh.exe"):
    target = os.path.join(bins, exe)
    if not os.path.exists(target):
        shutil.copy2(os.path.join(build, exe), target)
env = dict(os.environ, PATH=build + os.pathsep + os.environ.get("PATH", ""))

HDR = 272
BINARY = {b"NODE", b"SUPP", b"BARS", b"COLS", b"SLAB", b"WALL", b"FNDN", b"TRUS", b"CABL", b"LOAD", b"SNAP"}
JSONC = {b"BIMM", b"MESH", b"SURF", b"SETT", b"GRID", b"COOR", b"TOPO", b"DIMS", b"VIEW", b"OPNG", b"MVIS", b"XTND", b"SECP"}


def parse(path):
    raw = open(path, "rb").read()
    h = bytearray(raw[:HDR])
    flags = struct.unpack_from("<I", h, 20)[0]
    fsize = struct.unpack_from("<Q", h, 32)[0]
    payload = raw[HDR:fsize]
    tail = raw[fsize:]
    assert zlib.crc32(payload) == struct.unpack_from("<I", h, 28)[0], "CRC standard attendu"
    assert flags & 1, "fichier compressé attendu"
    data = zlib.decompress(payload[4:])
    chunks, off = [], 0
    while off + 16 <= len(data):
        cid, size, count, res = struct.unpack_from("<4sIII", data, off)
        off += 16
        chunks.append([cid, size, count, res, bytearray(data[off:off + size])])
        off += size
    return h, tail, chunks


def build_file(h, tail, chunks, trunc=None):
    out = bytearray()
    for cid, size, count, res, body in chunks:
        out += struct.pack("<4sIII", cid, size, count, res) + body
    if trunc:
        out = out[:trunc]
    payload = struct.pack(">I", len(out)) + zlib.compress(bytes(out), 6)
    head = bytearray(h)
    struct.pack_into("<I", head, 28, zlib.crc32(payload))
    struct.pack_into("<Q", head, 32, HDR + len(payload))
    struct.pack_into("<Q", head, 40, len(out))
    return bytes(head) + payload + tail


def fix_sizes(chunks):
    for c in chunks:
        c[1] = len(c[4])


SPECIALS = [struct.pack("<d", v) for v in (float("nan"), float("inf"), -float("inf"), 1e308, -1e308, 0.0, 5e-324)]
INTS = [struct.pack("<i", v) for v in (0, -1, 1, 2147483647, -2147483648, 999999, 3)]


def mutate(h, chunks):
    """Renvoie (description, troncature éventuelle)."""
    kinds = ["flip", "flip", "int", "int", "double", "count", "size", "drop", "dup", "json", "json", "trunc", "enumbyte"]
    k = rng.choice(kinds)
    present = [c for c in chunks if c[0] in BINARY and c[4]]
    jsons = [c for c in chunks if c[0] in JSONC and c[4]]
    if k == "flip":
        c = rng.choice(present)
        n = rng.choice([1, 1, 2, 4, 8, 32])
        for _ in range(n):
            c[4][rng.randrange(len(c[4]))] = rng.randrange(256)
        return f"flip {c[0].decode()} x{n}", None
    if k == "enumbyte":
        c = rng.choice(present)
        p = rng.randrange(len(c[4]))
        c[4][p] = rng.choice([0xFF, 0x7F, 0x80, 200, 99])
        return f"octet {c[0].decode()}@{p}", None
    if k == "int":
        c = rng.choice(present)
        if len(c[4]) < 8:
            return "noop", None
        p = rng.randrange(0, len(c[4]) - 4)
        c[4][p:p + 4] = rng.choice(INTS)
        return f"int {c[0].decode()}@{p}", None
    if k == "double":
        c = rng.choice(present)
        if len(c[4]) < 16:
            return "noop", None
        p = rng.randrange(0, len(c[4]) - 8)
        c[4][p:p + 8] = rng.choice(SPECIALS)
        return f"double {c[0].decode()}@{p}", None
    if k == "count":
        c = rng.choice(present)
        c[2] = rng.choice([0, c[2] + 1, c[2] * 2, 1000000, 0xFFFFFFFF, max(0, c[2] - 1)])
        return f"count {c[0].decode()}={c[2]}", None
    if k == "size":
        c = rng.choice(chunks)
        c[1] = rng.choice([0, c[1] + 1, max(0, c[1] - 1), 0xFFFFFFFF, c[1] * 2])
        return f"size {c[0].decode()}={c[1]}", None
    if k == "drop":
        c = rng.choice(chunks)
        chunks.remove(c)
        return f"suppression {c[0].decode()}", None
    if k == "dup":
        c = rng.choice(chunks)
        chunks.insert(rng.randrange(len(chunks) + 1), [c[0], c[1], c[2], c[3], bytearray(c[4])])
        return f"doublon {c[0].decode()}", None
    if k == "json":
        if not jsons:
            return "noop", None
        c = rng.choice(jsons)
        txt = c[4]
        for _ in range(rng.choice([1, 3, 10])):
            p = rng.randrange(len(txt))
            txt[p] = rng.choice(b'{}[]",:0-9e"nul')
        c[1] = len(txt)
        return f"json {c[0].decode()}", None
    if k == "trunc":
        return "troncature", rng.randrange(1, sum(16 + len(c[4]) for c in chunks))
    return "noop", None


CODES = {0xC0000005: "ACCESS_VIOLATION", 0xC0000409: "STACK_BUFFER_OVERRUN/FAILFAST", 0xC0000374: "HEAP_CORRUPTION", 0xC00000FD: "STACK_OVERFLOW",
         0xC000001D: "ILLEGAL_INSTRUCTION", 0xE06D7363: "CPP_EXCEPTION", 0xC0000094: "INT_DIVIDE_BY_ZERO", 0xC0000017: "NO_MEMORY", 3: "ABORT/TERMINATE"}


def run(cmd, timeout):
    t = time.monotonic()
    try:
        p = subprocess.run(cmd, cwd=work, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout, creationflags=subprocess.CREATE_NO_WINDOW)
        return p.returncode & 0xFFFFFFFF, p.stdout[-400:].decode("utf-8", "replace"), time.monotonic() - t
    except subprocess.TimeoutExpired:
        subprocess.run(["taskkill", "/F", "/IM", os.path.basename(cmd[0])], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return None, "DÉLAI", time.monotonic() - t


h0, tail0, chunks0 = parse(seed_file)
print("semence", seed_file, "chunks", [(c[0].decode(), c[1], c[2]) for c in chunks0], flush=True)
crashes = os.path.join(work, "crashes")
os.makedirs(crashes, exist_ok=True)
stats, journal = collections.Counter(), []
candidate = os.path.join(work, "candidat.tsa")
# témoin : la semence non mutée doit passer
for target in ("ifc", "mesh"):
    data = build_file(h0, tail0, [[c[0], c[1], c[2], c[3], bytearray(c[4])] for c in chunks0])
    open(candidate, "wb").write(data)
    cmd = [os.path.join(bins, "tsa-ifc.exe"), "export", candidate, os.path.join(work, "o.ifc")] if target == "ifc" else [os.path.join(bins, "tsa-mesh.exe"), candidate, os.path.join(work, "o.msh"), "2.0"]
    rc, txt, dur = run(cmd, 120)
    print("témoin", target, "code", rc, f"{dur:.2f}s", txt[-160:].replace("\n", " | "), flush=True)

t0 = time.monotonic()
for it in range(iterations):
    chunks = [[c[0], c[1], c[2], c[3], bytearray(c[4])] for c in chunks0]
    n = rng.choice([1, 1, 1, 2, 3])
    descs, trunc = [], None
    for _ in range(n):
        d, tr = mutate(h0, chunks)
        descs.append(d)
        trunc = tr or trunc
    # la plupart des mutations gardent des tailles cohérentes pour aller loin dans le lecteur
    if not any(d.startswith("size") for d in descs):
        fix_sizes(chunks)
    data = build_file(h0, tail0, chunks, trunc)
    open(candidate, "wb").write(data)
    harness = os.environ.get("AUDIT_HARNESS")
    if harness:
        target = "analyse"
        cmd = [harness, "fuzzload", candidate]
    else:
        target = "ifc" if it % 4 else "mesh"
        cmd = [os.path.join(bins, "tsa-ifc.exe"), "export", candidate, os.path.join(work, "o.ifc")] if target == "ifc" else [os.path.join(bins, "tsa-mesh.exe"), candidate, os.path.join(work, "o.msh"), "4.0"]
    rc, txt, dur = run(cmd, 40)
    if rc is None:
        label = "DÉLAI"
    elif rc in (0, 1, 2):
        label = f"géré({rc})"
    else:
        label = CODES.get(rc, hex(rc))
    stats[(target, label)] += 1
    if not label.startswith("géré"):
        name = f"{target}_{label.split('/')[0]}_{it:05d}.tsa"
        shutil.copy2(candidate, os.path.join(crashes, name))
        journal.append(dict(it=it, target=target, label=label, mutations=descs, sortie=txt[-200:]))
        print(it, target, label, descs, flush=True)
    if it and it % 100 == 0:
        print("…", it, dict(collections.Counter(l for (_, l) in stats.elements())), f"{time.monotonic() - t0:.0f}s", flush=True)

print("BILAN", iterations, "itérations,", f"{time.monotonic() - t0:.0f}s")
for k, v in sorted(stats.items()):
    print(" ", k, v)
json.dump(dict(iterations=iterations, graine=graine, bilan={f"{a}|{b}": v for (a, b), v in stats.items()}, plantages=journal), open(os.path.join(work, "bilan.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
