#!/usr/bin/env python3
"""Contrôle des traductions de TSA (docs/I18N.md §5).

  python tools/i18n/check_translations.py [--strict] [--report docs/I18N_STATUS.md] [--details N]

Pour translations/tsa_en.ts et translations/tsa_mg.ts :
  - textes traduits / non traduits (par contexte), couverture ;
  - erreurs : traduction terminée mais vide, paramètres %1/%n ou raccourci & différents de la source, doublon de texte
    dans un contexte, caractère corrompu (« � », séquence UTF-8 mal décodée « Ã© ») ;
  - littéraux français hors tr() encore présents dans src/ (textes qui resteraient en français après un changement de
    langue), comptés par dossier.
Code de sortie 1 avec --strict si une erreur est trouvée (la couverture seule n'est jamais une erreur : elle est
mesurée et rapportée, jamais masquée).
"""
import argparse
import collections
import datetime
import os
import re
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LANGUAGES = {"en": "English", "mg": "Malagasy"}
PLACEHOLDER = re.compile(r"%L?\d+|%n")
ACCEL = re.compile(r"&(?!&)(?=\S)")
MOJIBAKE = re.compile(r"�|Ã[\x80-\xbf©¨ª«®§´]|â€")
FRENCH = re.compile(r"[àâçéèêëîïôûùüœ]|\b(le|la|les|du|des|une?|de|et|pour|sur|avec|aucun|aucune|est|sont)\b", re.I)
LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')


def accelerators(text):
    return len(ACCEL.findall((text or "").replace("&&", "")))


def check_ts(path):
    root = ET.parse(path).getroot()
    stats = {"total": 0, "done": 0, "per_context": collections.Counter(), "missing_per_context": collections.Counter()}
    errors, missing = [], []
    for ctx in root.iter("context"):
        name = ctx.findtext("name")
        seen = set()
        for msg in ctx.iter("message"):
            source = msg.findtext("source") or ""
            stats["total"] += 1
            stats["per_context"][name] += 1
            key = (source, msg.findtext("comment") or "")
            if key in seen:
                errors.append(f"[{name}] doublon : {source!r}")
            seen.add(key)
            tr = msg.find("translation")
            if tr is None:
                missing.append((name, source))
                stats["missing_per_context"][name] += 1
                continue
            forms = [f.text or "" for f in tr.findall("numerusform")] if msg.get("numerus") == "yes" else [tr.text or ""]
            unfinished = tr.get("type") in ("unfinished", "vanished", "obsolete")
            if unfinished or not any(forms):
                if not unfinished and not any(forms):
                    errors.append(f"[{name}] traduction terminée mais vide : {source!r}")
                missing.append((name, source))
                stats["missing_per_context"][name] += 1
                continue
            stats["done"] += 1
            for f in forms:
                if sorted(PLACEHOLDER.findall(source)) != sorted(PLACEHOLDER.findall(f)) and "%n" not in source:
                    errors.append(f"[{name}] paramètres différents : {source!r} → {f!r}")
                if accelerators(source) != accelerators(f):
                    errors.append(f"[{name}] raccourci & différent : {source!r} → {f!r}")
                if MOJIBAKE.search(f):
                    errors.append(f"[{name}] caractère corrompu : {f!r}")
    return stats, errors, missing


def scan_untranslatable(src):
    per_dir = collections.Counter()
    for d, _, files in os.walk(src):
        for fn in files:
            if not fn.endswith((".cpp", ".h")):
                continue
            path = os.path.join(d, fn)
            rel = os.path.relpath(path, src).replace(os.sep, "/")
            for line in open(path, encoding="utf-8", errors="replace"):
                s = line.strip()
                if s.startswith(("//", "*", "/*")) or "tr(" in line or "TSA_LOG" in line or "translate(" in line or "QT_TR" in line:
                    continue
                code = line.split("//")[0]
                for m in LITERAL.finditer(code):
                    t = m.group(1)
                    if len(t) >= 4 and re.search(r"[A-Za-zÀ-ÿ]{3,}", t) and FRENCH.search(t):
                        per_dir[rel.split("/")[0]] += 1
    return per_dir


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--report")
    parser.add_argument("--details", type=int, default=15)
    args = parser.parse_args()

    total_errors = 0
    report = [f"# État des traductions\n", f"Produit par `tools/i18n/check_translations.py --report` le "
              f"{datetime.date.today().isoformat()}. Langue source : français ([I18N.md](I18N.md)).\n"]
    summary_rows = []
    context_rows = collections.defaultdict(dict)
    for code, label in LANGUAGES.items():
        path = os.path.join(ROOT, "translations", f"tsa_{code}.ts")
        stats, errors, missing = check_ts(path)
        total_errors += len(errors)
        pct = 100.0 * stats["done"] / max(1, stats["total"])
        print(f"{label} ({code}) : {stats['done']} / {stats['total']} textes traduits ({pct:.1f} %), "
              f"{len(missing)} manquant(s), {len(errors)} erreur(s)")
        for e in errors[: args.details]:
            print("   ERREUR " + e)
        worst = stats["missing_per_context"].most_common(args.details)
        if worst:
            print("   contextes les moins traduits : " + ", ".join(f"{c} ({n})" for c, n in worst))
        summary_rows.append(f"| {label} | {stats['done']} | {stats['total']} | {pct:.1f} % | {len(missing)} | {len(errors)} |")
        for ctx, n in stats["per_context"].items():
            context_rows[ctx][code] = (n - stats["missing_per_context"][ctx], n)

    per_dir = scan_untranslatable(os.path.join(ROOT, "src"))
    hard = sum(per_dir.values())
    print(f"Littéraux français hors tr() (resteraient en français) : {hard} — " +
          ", ".join(f"{d} {n}" for d, n in per_dir.most_common(12)))

    if args.report:
        report += ["\n## Textes extraits du code (tr, translate, QT_TRANSLATE_NOOP)\n",
                   "| Langue | Traduits | Total | Couverture | Manquants | Erreurs |", "|---|---|---|---|---|---|"]
        report += summary_rows
        report += ["\n## Par contexte (classe ou zone de l'interface)\n",
                   "| Contexte | English | Malagasy |", "|---|---|---|"]
        for ctx in sorted(context_rows, key=lambda c: -context_rows[c].get("mg", (0, 0))[1]):
            cells = []
            for code in LANGUAGES:
                done, n = context_rows[ctx].get(code, (0, 0))
                cells.append(f"{done} / {n}" + (" ✓" if done == n else ""))
            report.append(f"| `{ctx}` | " + " | ".join(cells) + " |")
        report += ["\n## Textes encore écrits hors `tr()`\n",
                   "Littéraux français du code qui ne passent pas par le système de traduction : ils restent en "
                   f"français quelle que soit la langue choisie. Total : **{hard}** (heuristique : chaînes contenant "
                   "des mots ou accents français ; une partie n'est jamais affichée : scripts Tcl, consignes envoyées "
                   "à l'IA, journaux techniques).\n",
                   "| Dossier de `src/` | Littéraux |", "|---|---|"]
        report += [f"| `{d}` | {n} |" for d, n in per_dir.most_common()]
        with open(os.path.join(ROOT, args.report), "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(report) + "\n")
        print(f"Rapport : {args.report}")

    return 1 if (args.strict and total_errors) else 0


if __name__ == "__main__":
    sys.exit(main())
