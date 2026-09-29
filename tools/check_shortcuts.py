#!/usr/bin/env python3
"""Verifie les raccourcis de src/Commands/CommandCatalog.cpp.

Erreurs (exit 1) : raccourci partage par plusieurs commandes (ambigu sous Qt,
aucune des commandes ne se declenche), nom de touche non portable.
Avertissements : ecarts avec docs/shortcuts.txt (--strict pour les rendre bloquants).

Usage : python tools/check_shortcuts.py [--strict]
"""
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CATALOG = ROOT / "src" / "Commands" / "CommandCatalog.cpp"
DOCS = ROOT / "docs" / "shortcuts.txt"

# Noms francais -> noms portables Qt
ALIASES = {
    "suppr": "del",
    "echap": "esc",
    "num1": "num+1",
    "num3": "num+3",
    "num5": "num+5",
    "num7": "num+7",
}
NON_PORTABLE = {"suppr", "echap"}

CALL = re.compile(
    r'registerCommand\(\{\s*"(cmd[^"]+)",\s*"(?:[^"\\]|\\.)*",\s*"(?:[^"\\]|\\.)*",\s*"([^"]*)"',
    re.S,
)


def norm(seq: str) -> str:
    parts = [ALIASES.get(p.strip().lower(), p.strip().lower()) for p in seq.split("+") if p.strip()]
    return "+".join(parts) if parts else seq.strip().lower()


def catalog_shortcuts():
    text = CATALOG.read_text(encoding="utf-8")
    return {cid: sc for cid, sc in CALL.findall(text) if sc}


def doc_shortcuts():
    found = set()
    for line in DOCS.read_text(encoding="utf-8-sig").splitlines():
        if "RESUME RAPIDE" in line:
            break
        m = re.match(r"^  (\S.*?)\s{2,}(\S.*)$", line)
        if not m or m.group(2).startswith("=") or m.group(1) == "[aucun]" or "alternatif" in m.group(2).lower():
            continue
        found.add(norm(m.group(1)))
    return found


def main() -> int:
    strict = "--strict" in sys.argv
    cat = catalog_shortcuts()
    errors, warnings = [], []

    by_key = defaultdict(list)
    for cid, sc in cat.items():
        by_key[norm(sc)].append(cid)
        if any(p.strip().lower() in NON_PORTABLE for p in sc.split("+")):
            errors.append(f"Nom de touche non portable '{sc}' ({cid}) : utiliser Del/Esc")
    for key, ids in by_key.items():
        if len(ids) > 1:
            errors.append(f"Raccourci '{key}' partage par : {', '.join(ids)}")

    docs = doc_shortcuts()
    cats = set(by_key)
    for k in sorted(docs - cats):
        warnings.append(f"Documente mais absent du catalogue : {k}")
    for k in sorted(cats - docs):
        warnings.append(f"Dans le catalogue mais non documente : {k}")

    for e in errors:
        print("ERREUR :", e)
    for w in warnings:
        print("AVERT. :", w)
    if not errors and not warnings:
        print("OK : raccourcis coherents.")
    return 1 if errors or (strict and warnings) else 0


if __name__ == "__main__":
    sys.exit(main())
