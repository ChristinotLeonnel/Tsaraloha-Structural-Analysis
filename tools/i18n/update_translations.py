#!/usr/bin/env python3
"""Met à jour les fichiers de traduction de TSA (docs/I18N.md) à partir du code source.

  python tools/i18n/update_translations.py [--lupdate <chemin de lupdate.exe>]

Extrait les textes de src/ (tr(), QCoreApplication::translate, QT_TRANSLATE_NOOP) vers translations/tsa_en.ts et
translations/tsa_mg.ts. Les traductions existantes sont conservées ; un texte disparu du code est retiré
(-no-obsolete) ; aucun numéro de ligne n'est écrit (-locations none) pour que les .ts ne changent qu'avec les textes.
Langue source : le français. Ensuite : traduire dans Qt Linguist, puis tools/i18n/check_translations.py.
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LANGUAGES = ("en", "mg")


def find_lupdate(explicit):
    if explicit:
        return explicit
    for var in ("QTDIR", "Qt6_DIR", "QT_ROOT_DIR"):
        base = os.environ.get(var)
        if base:
            for candidate in (os.path.join(base, "bin", "lupdate.exe"), os.path.join(base, "..", "..", "..", "bin", "lupdate.exe")):
                if os.path.exists(candidate):
                    return os.path.abspath(candidate)
    found = shutil.which("lupdate")
    if found:
        return found
    matches = sorted(glob.glob(r"C:\Qt\6.*\msvc*_64\bin\lupdate.exe"), reverse=True)
    return matches[0] if matches else None


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--lupdate")
    args = parser.parse_args()
    lupdate = find_lupdate(args.lupdate)
    if not lupdate:
        print("lupdate introuvable : indiquer --lupdate ou QTDIR", file=sys.stderr)
        return 1
    os.makedirs(os.path.join(ROOT, "translations"), exist_ok=True)
    for lang in LANGUAGES:
        ts = os.path.join(ROOT, "translations", f"tsa_{lang}.ts")
        cmd = [lupdate, "-silent", "-source-language", "fr", "-target-language", lang, "-no-obsolete",
               "-locations", "none", "-no-ui-lines", os.path.join(ROOT, "src"), "-ts", ts]
        result = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
        if result.returncode != 0:
            print(result.stdout, result.stderr, file=sys.stderr)
            return result.returncode
        for line in (result.stdout + result.stderr).splitlines():
            if "lacks Q_OBJECT" in line or "Cannot invoke" in line or "Warning" in line:
                print(line)
        print(f"{os.path.relpath(ts, ROOT)} : à jour")
    return 0


if __name__ == "__main__":
    sys.exit(main())
