#!/usr/bin/env python3
"""Compare deux rapports TSA_Benchmark (latest.json) vérification par vérification : valeur calculée et statut.
Usage : compare_bench.py <avant/latest.json> <après/latest.json>"""
import json
import sys


def load(path):
    d = json.load(open(path, encoding="utf-8"))
    out = {}
    for c in d["cases"]:
        engine = c["engine"].split(" ")[0]
        for i, k in enumerate(c.get("checks", [])):
            out[(c["id"], engine, k["quantity"], i)] = (k.get("value"), k.get("status"))
        if not c.get("checks"):
            out[(c["id"], engine, "(cas)", 0)] = (None, "NON EXÉCUTÉ" if not c.get("ran") else "SANS VÉRIFICATION")
    return out


a, b = load(sys.argv[1]), load(sys.argv[2])
common = sorted(set(a) & set(b))
identical = same_status = 0
worst, where = 0.0, None
changed = []
for k in common:
    (va, sa), (vb, sb) = a[k], b[k]
    if sa == sb:
        same_status += 1
    else:
        changed.append((k, sa, sb))
    if va == vb:
        identical += 1
    elif isinstance(va, (int, float)) and isinstance(vb, (int, float)):
        d = abs(va - vb) / max(1.0, abs(va))
        if d > worst:
            worst, where = d, k
print("vérifications communes : %d ; valeurs identiques : %d ; statut identique : %d ; écart relatif max %.3g %s"
      % (len(common), identical, same_status, worst, where or ""))
print("seulement avant : %d ; seulement après : %d" % (len(set(a) - set(b)), len(set(b) - set(a))))
cases_before = sorted({k[0] for k in set(a) - set(b)})
cases_after = sorted({k[0] for k in set(b) - set(a)})
if cases_before:
    print("cas seulement avant :", ", ".join(cases_before))
if cases_after:
    print("cas seulement après :", ", ".join(cases_after))
for k, sa, sb in changed[:20]:
    print("statut changé", k, sa, "->", sb)
