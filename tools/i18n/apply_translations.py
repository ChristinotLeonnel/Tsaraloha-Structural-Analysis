#!/usr/bin/env python3
"""Applique des traductions à un fichier Qt Linguist de TSA (docs/I18N.md).

  python tools/i18n/apply_translations.py translations/tsa_mg.ts traductions.json [--only-unfinished]

traductions.json : { "<contexte>": { "<texte source>": "<traduction>" | ["<forme 1>", "<forme 2>"] } }
Le contexte « * » s'applique à tous les contextes (même texte source, même traduction).
Une traduction n'est écrite que si ses paramètres (%1, %n, %L1…) et son raccourci clavier (&) correspondent à ceux du
texte source ; les refus et les textes inconnus sont listés. Les traductions écrites sont marquées terminées.
Qt Linguist reste l'outil de relecture ; ce script sert aux traductions par lots.
"""
import json
import re
import sys
import xml.etree.ElementTree as ET

PLACEHOLDER = re.compile(r"%L?\d+|%n")
ACCEL = re.compile(r"&(?!&)(?=\S)")


def placeholders(text):
    return sorted(PLACEHOLDER.findall(text or ""))


def accelerators(text):
    return len(ACCEL.findall((text or "").replace("&&", "")))


def check(source, translation):
    if placeholders(source) != placeholders(translation):
        return "paramètres différents (%s / %s)" % (placeholders(source), placeholders(translation))
    if accelerators(source) != accelerators(translation):
        return "raccourci & absent ou en trop"
    if source.endswith("\n") != translation.endswith("\n"):
        return "fin de ligne différente"
    return None


def write_ts(path, root):
    body = ET.tostring(root, encoding="unicode")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write('<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>\n')
        f.write(body)
        f.write("\n")


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    ts_path, data_path = sys.argv[1], sys.argv[2]
    only_unfinished = "--only-unfinished" in sys.argv
    data = json.load(open(data_path, encoding="utf-8"))
    tree = ET.parse(ts_path)
    root = tree.getroot()
    used = set()
    written = refused = 0
    for ctx in root.iter("context"):
        name = ctx.findtext("name")
        table = dict(data.get("*", {}))
        table.update(data.get(name, {}))
        for msg in ctx.iter("message"):
            source = msg.findtext("source") or ""
            if source not in table:
                continue
            tr_el = msg.find("translation")
            if only_unfinished and tr_el is not None and tr_el.get("type") != "unfinished" and (tr_el.text or list(tr_el)):
                continue
            value = table[source]
            used.add((name if source in data.get(name, {}) else "*", source))
            forms = value if isinstance(value, list) else [value]
            problem = next((p for p in (check(source, f) for f in forms) if p), None)
            if problem:
                refused += 1
                print(f"REFUS [{name}] {source!r} : {problem}")
                continue
            if tr_el is None:
                tr_el = ET.SubElement(msg, "translation")
            for child in list(tr_el):
                tr_el.remove(child)
            if msg.get("numerus") == "yes":
                tr_el.text = None
                for f in forms:
                    ET.SubElement(tr_el, "numerusform").text = f
            else:
                tr_el.text = forms[0]
            tr_el.attrib.pop("type", None)
            written += 1
    unknown = [(c, s) for c, entries in data.items() for s in entries if (c, s) not in used and ("*", s) not in used]
    for c, s in unknown[:40]:
        print(f"INCONNU [{c}] {s!r}")
    write_ts(ts_path, root)
    print(f"{ts_path} : {written} traduction(s) écrite(s), {refused} refusée(s), {len(unknown)} texte(s) inconnu(s)")
    return 1 if refused else 0


if __name__ == "__main__":
    sys.exit(main())
