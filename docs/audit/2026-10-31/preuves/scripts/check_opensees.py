#!/usr/bin/env python3
"""Compare deux installations d'OpenSees et les essaie SANS aucune fenêtre (CREATE_NO_WINDOW, erreurs système muettes)."""
import ctypes, hashlib, os, subprocess, sys, tempfile

ctypes.windll.kernel32.SetErrorMode(0x8003)   # SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX
a, b, work = sys.argv[1], sys.argv[2], sys.argv[3]


def tree(root):
    out = {}
    for d, _, fs in os.walk(root):
        for f in fs:
            p = os.path.join(d, f)
            rel = os.path.relpath(p, root).replace("\\", "/").lower()
            h = hashlib.sha256()
            with open(p, "rb") as fh:
                for block in iter(lambda: fh.read(1 << 20), b""):
                    h.update(block)
            out[rel] = (os.path.getsize(p), h.hexdigest())
    return out


ta, tb = tree(a), tree(b)
same = [k for k in ta if k in tb and ta[k][1] == tb[k][1]]
print(f"{a} : {len(ta)} fichiers ; {b} : {len(tb)} fichiers ; identiques : {len(same)}")
print("seulement dans le premier :", sorted(set(ta) - set(tb))[:12])
print("seulement dans le second  :", sorted(set(tb) - set(ta))[:12])
print("différents :", sorted(k for k in ta if k in tb and ta[k][1] != tb[k][1])[:12])
print("OpenSees.exe sha256 :", ta.get("bin/opensees.exe", ("?", "?"))[1][:16], tb.get("bin/opensees.exe", ("?", "?"))[1][:16])

os.makedirs(work, exist_ok=True)
tcl = os.path.join(work, "essai.tcl")
open(tcl, "w").write('puts "VERSION [version]"\nmodel BasicBuilder -ndm 2 -ndf 3\nnode 1 0 0\nwipe\nputs TSA_AUDIT_OK\nexit\n')
for root in (a, b):
    exe = os.path.join(root, "bin", "OpenSees.exe")
    try:
        p = subprocess.run([exe, tcl], cwd=work, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=45, creationflags=subprocess.CREATE_NO_WINDOW)
        t = p.stdout.decode("utf-8", "replace")
        print(root, "code", hex(p.returncode & 0xFFFFFFFF), "ok" if "TSA_AUDIT_OK" in t else "SANS MARQUEUR", [l for l in t.splitlines() if "VERSION" in l or "Version" in l][:2], t[-160:].replace("\n", " | ") if "TSA_AUDIT_OK" not in t else "")
    except subprocess.TimeoutExpired:
        print(root, "DÉLAI DÉPASSÉ (45 s)")
        subprocess.run(["taskkill", "/F", "/IM", "OpenSees.exe"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW)
