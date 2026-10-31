#!/usr/bin/env python3
"""Compare deux empreintes du harnais (dump_all.py) fichier par fichier : identiques à l'octet, ou différences listées.
Pour un script Tcl, les lignes qui diffèrent sont montrées ; pour un fichier de résultats, l'écart numérique maximal.
Usage : compare_dumps.py <avant> <après>"""
import difflib
import os
import re
import sys

before, after = sys.argv[1], sys.argv[2]
names = sorted(set(os.listdir(before)) | set(os.listdir(after)))
NUM = re.compile(r"[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?")
identical = differ = missing = 0
for n in names:
    a, b = os.path.join(before, n), os.path.join(after, n)
    if not (os.path.exists(a) and os.path.exists(b)):
        missing += 1
        print("ABSENT", n, "avant" if not os.path.exists(a) else "après")
        continue
    da, db = open(a, "rb").read(), open(b, "rb").read()
    if da == db:
        identical += 1
        continue
    differ += 1
    ta, tb = da.decode("utf-8", "replace").splitlines(), db.decode("utf-8", "replace").splitlines()
    if n.endswith(".tcl"):
        print("DIFFÉRENT", n)
        for line in list(difflib.unified_diff(ta, tb, lineterm="", n=0))[2:40]:
            print("   ", line[:200])
        continue
    worst, where = 0.0, ""
    same_shape = len(ta) == len(tb)
    for la, lb in zip(ta, tb):
        na, nb = NUM.findall(la), NUM.findall(lb)
        if len(na) != len(nb):
            same_shape = False
            continue
        for x, y in zip(na, nb):
            fx, fy = float(x), float(y)
            d = abs(fx - fy) / max(1.0, abs(fx))
            if d > worst:
                worst, where = d, la[:120]
    print("DIFFÉRENT", n, "lignes %d/%d" % (len(ta), len(tb)), "forme identique" if same_shape else "FORME DIFFÉRENTE",
          "écart relatif max %.3g" % worst, ("à : " + where) if where else "")
    if not same_shape:
        for line in list(difflib.unified_diff(ta, tb, lineterm="", n=0))[2:12]:
            print("   ", line[:200])
print("fichiers : %d identiques, %d différents, %d absents d'un côté" % (identical, differ, missing))
