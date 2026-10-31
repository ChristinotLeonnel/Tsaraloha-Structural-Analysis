#!/usr/bin/env python3
"""Reprend dans tests/benchmarks les cas de l'audit du 2026-10-31 qui passent aujourd'hui, avec les seuls moteurs qui
les passent (OpenSees exclu des rotules sous charge non uniforme tant que BUG-148 est ouvert).

Usage : make_bench_cases.py <racine TSA> <dossier des portiques aléatoires>
"""
import json
import os
import sys

root, random_dir = sys.argv[1], sys.argv[2]
hinges = os.path.join(root, "docs", "audit", "2026-10-31", "preuves", "cas", "rotules")
ALL = ["opensees", "fem3d", "custom2d"]
NO_OPS = ["fem3d", "custom2d"]
BUG148 = ("OpenSees exclu tant que BUG-148 est ouvert (rotule + charge ponctuelle, trapézoïdale ou triangulaire : "
          "efforts faux sans alerte). ")

cases = [
    # (source, dossier, nouvel id, catégorie, moteurs, note)
    ("HINGE-01-uniform", hinges, "RDM-13a", "rdm", ALL, ""),
    ("GERBER-01-uniform", hinges, "RDM-13b", "rdm", ALL, ""),
    ("PIN2-uniform", hinges, "RDM-13c", "rdm", ALL, ""),
    ("HINGE-02-point", hinges, "RDM-14a", "rdm", NO_OPS, BUG148),
    ("HINGE-03-trap-constant", hinges, "RDM-14b", "rdm", NO_OPS, BUG148),
    ("HINGE-04-tri-up", hinges, "RDM-14c", "rdm", NO_OPS, BUG148),
    ("PIN2-point", hinges, "RDM-14d", "rdm", NO_OPS, BUG148),
    ("RND-A072", random_dir, "FEA-08", "fea", ["opensees", "fem3d"], ""),
    ("RND-B087", random_dir, "FEA-09", "fea", ["opensees", "fem3d"], ""),
    ("RND-C074", random_dir, "FEA-10", "fea", ["fem3d"], "OpenSees exclu tant que BUG-148 est ouvert (portique à rotules "
                                                          "avec charges ponctuelles et trapézoïdales). "),
]

for source, folder, new_id, category, engines, note in cases:
    with open(os.path.join(folder, source + ".json"), encoding="utf-8") as f:
        case = json.load(f)
    case["id"] = new_id
    case["category"] = category
    case["engines"] = engines
    origin = "Cas « %s » de l'audit technique du 2026-10-31 (docs/audit/2026-10-31/preuves). " % source
    if folder == random_dir:
        origin += ("Portique régénéré à l'identique par preuves/scripts/gen_cases_random.py (graine fixe), valeurs de "
                   "référence du solveur indépendant preuves/scripts/frame3d.py. ")
    case["description"] = (case.get("description", "").rstrip(". ") + ". " if case.get("description") else "") + origin + note
    case["description"] = case["description"].strip()
    out = os.path.join(root, "tests", "benchmarks", category, new_id + ".json")
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        json.dump(case, f, ensure_ascii=False, indent=1)
        f.write("\n")
    print("%-24s -> %s (%s, %d vérifications)" % (source, os.path.relpath(out, root), ", ".join(engines), len(case["checks"])))
