#!/usr/bin/env python3
"""Vérifie les raccourcis par défaut de src/Commands/CommandCatalog.cpp.

Erreurs (exit 1) : raccourci partagé par plusieurs commandes, raccourci qui commence une suite de touches
d'une autre commande (« D » et « D, A » : Qt attendrait la touche suivante), nom de touche non portable.
Avertissements : écarts avec la référence générée docs/shortcut.txt (--strict pour les rendre bloquants ;
régénérer avec TSA_UPDATE_SHORTCUT_REFERENCE=1 TSA_TestSuite --suite=shortcuts).

Usage : python tools/check_shortcuts.py [--strict]
"""
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CATALOG = ROOT / "src" / "Commands" / "CommandCatalog.cpp"
REFERENCE = ROOT / "docs" / "shortcut.txt"
NON_PORTABLE = {"suppr", "echap", "échap", "maj"}

CALL = re.compile(r'registerCommand\(\{"(cmd[^"]+)",\s*"(?:[^"\\]|\\.)*",\s*"(?:[^"\\]|\\.)*",\s*"([^"]*)"', re.S)


def split_aliases(text):
    out, cur = [], ""
    for ch in text:
        if ch == ";" and cur.strip() and not cur.strip().endswith("+"):
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def norm(seq):
    chords = []
    for chord in seq.split(","):
        parts = [p.strip().lower() for p in re.split(r"(?<!\+)\+(?!$)", chord.strip()) if p.strip()]
        mods = sorted(p for p in parts if p in {"ctrl", "shift", "alt", "meta", "num"})
        keys = [p for p in parts if p not in {"ctrl", "shift", "alt", "meta", "num"}]
        chords.append("+".join(mods + keys))
    return tuple(chords)


def catalog():
    text = CATALOG.read_text(encoding="utf-8")
    return {cid: split_aliases(sc) for cid, sc in CALL.findall(text)}


def reference():
    found = {}
    if not REFERENCE.exists():
        return None
    for line in REFERENCE.read_text(encoding="utf-8").splitlines():
        if not line.startswith("cmd."):
            continue
        fields = [f.strip() for f in line.split("|")]
        found[fields[0]] = split_aliases(fields[3] if len(fields) > 3 else "")
    return found


def main():
    strict = "--strict" in sys.argv
    cat = catalog()
    errors, warnings = [], []
    entries = []
    for cid, seqs in cat.items():
        for s in seqs:
            if any(p.strip().lower() in NON_PORTABLE for p in re.split(r"[+,]", s)):
                errors.append(f"Nom de touche non portable '{s}' ({cid}) : utiliser Del, Esc, Shift")
            entries.append((cid, s, norm(s)))
    by_key = defaultdict(list)
    for cid, s, n in entries:
        by_key[n].append(cid)
    for n, ids in by_key.items():
        if len(set(ids)) > 1:
            errors.append(f"Raccourci '{', '.join(n)}' partagé par : {', '.join(sorted(set(ids)))}")
    for cid_a, sa, na in entries:
        for cid_b, sb, nb in entries:
            if cid_a != cid_b and len(na) < len(nb) and nb[:len(na)] == na:
                errors.append(f"'{sa}' ({cid_a}) commence '{sb}' ({cid_b}) : '{sa}' ne se déclencherait jamais")

    ref = reference()
    if ref is None:
        warnings.append(f"Référence absente : {REFERENCE.relative_to(ROOT)}")
    else:
        for cid in sorted(set(cat) - set(ref)):
            warnings.append(f"Absente de docs/shortcut.txt : {cid}")
        for cid in sorted(set(ref) - set(cat)):
            warnings.append(f"Dans docs/shortcut.txt mais plus au catalogue : {cid}")
        for cid in sorted(set(cat) & set(ref)):
            if [norm(s) for s in cat[cid]] != [norm(s) for s in ref[cid]]:
                warnings.append(f"Défaut différent pour {cid} : catalogue {cat[cid]} / référence {ref[cid]}")

    for e in errors:
        print("ERREUR :", e)
    for w in warnings:
        print("AVERT. :", w)
    if not errors and not warnings:
        print(f"OK : {len(cat)} commandes, {len(entries)} raccourcis par défaut cohérents, référence à jour.")
    return 1 if errors or (strict and warnings) else 0


if __name__ == "__main__":
    sys.exit(main())
