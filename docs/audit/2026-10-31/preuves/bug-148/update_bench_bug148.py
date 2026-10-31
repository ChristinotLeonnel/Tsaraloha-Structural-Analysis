#!/usr/bin/env python3
"""BUG-148 corrigé : OpenSees réintégré dans les cas à rotule du banc, cas d'audit restants ajoutés.
Usage : update_bench_bug148.py <racine TSA>"""
import json
import os
import sys

root = sys.argv[1]
hinges = os.path.join(root, "docs", "audit", "2026-10-31", "preuves", "cas", "rotules")
rdm = os.path.join(root, "tests", "benchmarks", "rdm")
fea = os.path.join(root, "tests", "benchmarks", "fea")
ALL = ["opensees", "fem3d", "custom2d"]
NOTE = ("Non-régression de BUG-148 : charge non uniforme sur barre à rotule, OpenSees calcule ce cas par forces "
        "d'encastrement condensées.")


def write(path, case):
    text = json.dumps(case, ensure_ascii=False, indent=1) + "\n"
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text.replace("\n", "\r\n"))


def strip_exclusion(text):
    marker = " OpenSees exclu tant que BUG-148 est ouvert"
    at = text.find(marker)
    return text[:at].rstrip() if at >= 0 else text.rstrip()


for name in ("RDM-14a", "RDM-14b", "RDM-14c", "RDM-14d"):
    path = os.path.join(rdm, name + ".json")
    case = json.load(open(path, encoding="utf-8"))
    case["engines"] = ALL
    case["description"] = strip_exclusion(case["description"]) + " " + NOTE
    write(path, case)
    print(name, case["engines"])

path = os.path.join(fea, "FEA-10.json")
case = json.load(open(path, encoding="utf-8"))
case["engines"] = ["opensees", "fem3d"]
case["description"] = strip_exclusion(case["description"]) + " " + NOTE
write(path, case)
print("FEA-10", case["engines"])

for source, new_id in (("HINGE-05-tri-down", "RDM-14e"), ("GERBER-02-trap", "RDM-14f"), ("PIN2-trap", "RDM-14g")):
    case = json.load(open(os.path.join(hinges, source + ".json"), encoding="utf-8"))
    case["id"] = new_id
    case["category"] = "rdm"
    case["engines"] = ALL
    origin = "Cas « %s » de l'audit technique du 2026-10-31 (docs/audit/2026-10-31/preuves)." % source
    case["description"] = case.get("description", "").rstrip(". ") + ". " + origin + " " + NOTE
    write(os.path.join(rdm, new_id + ".json"), case)
    print(new_id, source, len(case["checks"]))
