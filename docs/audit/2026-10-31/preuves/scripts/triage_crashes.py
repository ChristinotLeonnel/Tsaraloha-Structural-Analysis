#!/usr/bin/env python3
"""Rejoue chaque fichier de plantage du fuzzing dans le harnais de diagnostic : dernière étape atteinte, sans fenêtre."""
import collections, ctypes, glob, os, re, subprocess, sys

ctypes.windll.kernel32.SetErrorMode(0x8003)
sp = sys.argv[1]
build = r"D:\Tsaraloha Platforme\TSA\build-windows-validation"
env = dict(os.environ, PATH=build + os.pathsep + os.environ.get("PATH", ""))
harness = os.path.join(sp, "harness", "harness.exe")
rows, bystage = [], collections.Counter()
for d in ("fuzz3", "fuzz4"):
    for f in sorted(glob.glob(os.path.join(sp, d, "crashes", "*.tsa"))):
        try:
            p = subprocess.run([harness, "fuzzdiag", f], cwd=os.path.join(sp, "harness-run"), env=env, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=60,
                               creationflags=subprocess.CREATE_NO_WINDOW)
            err = p.stderr.decode("utf-8", "replace")
            code = hex(p.returncode & 0xFFFFFFFF)
        except subprocess.TimeoutExpired:
            err, code = "", "DÉLAI"
        lines = [l.strip() for l in err.splitlines() if l.strip()]
        steps = [l for l in lines if re.match(r"E\d ", l)]
        loaded = next((l for l in lines if l.startswith("chargé")), "")
        exc = [l[:150] for l in lines if "EXCEPTION" in l]
        stage = steps[-1] if steps else "?"
        key = (code, stage, exc[0][:80] if exc else "")
        bystage[key] += 1
        rows.append((os.path.basename(f), code, stage, loaded, exc[:1]))
for r in rows:
    print(r)
print("\n--- synthèse")
for k, v in bystage.most_common():
    print(v, k)
