#!/usr/bin/env python3
"""Exécute TSA_TestSuite suite par suite et produit un rapport clair (intégration continue et poste local).

Chaque suite est lancée séparément (un échec n'empêche pas les autres suites de s'exécuter), dans un dossier de
travail jetable (les tests écrivent dans le dossier courant), avec un délai maximal. Statuts :
  RÉUSSIE         code de retour 0, aucune ligne [FAIL]
  ÉCHOUÉE         au moins une ligne [FAIL] ou code de retour non nul (dans une suite, le premier échec arrête
                  la suite : les tests suivants ne sont pas exécutés, BUG-104)
  DÉLAI DÉPASSÉ   arrêtée au bout de --timeout secondes
  PLANTÉE         arrêtée par un signal (code de retour négatif)
  NON EXÉCUTÉE    dépendance absente de cet environnement (OpenSees, Gmsh, IfcOpenShell, affichage X11,
                  Windows) : jamais comptée comme réussie
Les lignes [SKIP] (tests ignorés par la suite elle-même) sont rapportées à part.

Code de sortie : 0 si aucune suite exécutée n'a échoué, 1 sinon, 2 en cas d'erreur d'utilisation.

Exemple :
  python3 tools/ci/run_test_suites.py --exe build-linux-release/TSA_TestSuite --out ci-report \\
      --available opensees,gmsh,ifcopenshell,display
"""

import argparse
import datetime
import json
import os
import re
import shutil
import subprocess
import sys
import time

# Suites déclarées par tests/test_main.cpp (option --suite=), dans l'ordre d'exécution de « all ».
SUITES = [
    "coordinates", "model", "io", "commands", "grids", "viewer", "cables", "extensions", "workplane", "window", "node",
    "loads", "opensees", "supports", "standards", "ndc", "extraction", "ai", "operator", "ailive", "preview", "thumbnail",
    "engines", "tools", "mdd", "cleanup", "bim", "ifc", "drawing", "views", "snap", "automation", "blueprint", "help",
    "appearance", "groupedit", "topology", "dimensions", "displaymodes", "shortcuts", "tsa3d", "modules", "templates",
    "deployment", "mesh", "torsion", "meshview", "demo", "surfaces", "audit", "benchmarks", "display", "multidoc",
    "i18n", "ribbon", "templates_wp",
]

# Dépendances d'environnement par suite, constatées sous Linux le 2026-10-25 (exécution sans OpenSees, sans
# Gmsh, sans IfcOpenShell, sans affichage). À tenir à jour : une suite oubliée ici échoue (jamais masquée).
REQUIREMENTS = {
    "opensees": ["opensees"], "ndc": ["opensees"], "extraction": ["opensees"], "engines": ["opensees"],
    "tsa3d": ["opensees"], "templates": ["opensees"], "torsion": ["opensees"], "demo": ["opensees"],
    "audit": ["opensees"], "benchmarks": ["opensees"], "mdd": ["opensees"], "displaymodes": ["opensees"],
    "mesh": ["gmsh"], "surfaces": ["gmsh", "opensees"], "meshview": ["gmsh", "display"],
    "ifc": ["ifcopenshell"],
    "display": ["display"], "multidoc": ["display"],
}
KNOWN_CAPABILITIES = {"opensees", "gmsh", "ifcopenshell", "display"}

RESULTS_RE = re.compile(r"RESULTS:\s*(\d+)\s*test\(s\) passed")


def run_suite(exe, suite, workdir, timeout, env):
    os.makedirs(workdir, exist_ok=True)
    start = time.monotonic()
    try:
        proc = subprocess.run([exe, f"--suite={suite}"], cwd=workdir, env=env, capture_output=True, timeout=timeout)
        out = proc.stdout.decode("utf-8", "replace") + proc.stderr.decode("utf-8", "replace")
        code = proc.returncode
        timed_out = False
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b"").decode("utf-8", "replace") + (e.stderr or b"").decode("utf-8", "replace")
        code = None
        timed_out = True
    duration = time.monotonic() - start
    fails = [line.strip() for line in out.splitlines() if line.lstrip().startswith("[FAIL]")]
    skips = [line.strip() for line in out.splitlines() if line.lstrip().startswith("[SKIP]")]
    m = RESULTS_RE.findall(out)
    passed = int(m[-1]) if m else 0
    if timed_out:
        status = "DÉLAI DÉPASSÉ"
    elif code is not None and code < 0:
        status = "PLANTÉE"
    elif fails or code != 0:
        status = "ÉCHOUÉE"
    else:
        status = "RÉUSSIE"
    return {"suite": suite, "status": status, "passed": passed, "fails": fails, "skips": skips, "returncode": code,
            "seconds": round(duration, 2)}, out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", required=True, help="chemin de TSA_TestSuite")
    ap.add_argument("--out", required=True, help="dossier du rapport (créé) ; les suites s'y exécutent")
    ap.add_argument("--available", default="", help="dépendances présentes : opensees,gmsh,ifcopenshell,display")
    ap.add_argument("--suites", default="", help="sous-ensemble de suites (défaut : toutes)")
    ap.add_argument("--timeout", type=int, default=900, help="délai maximal par suite (s)")
    args = ap.parse_args()

    exe = os.path.abspath(args.exe)
    if not os.path.isfile(exe):
        print(f"Exécutable introuvable : {exe}", file=sys.stderr)
        return 2
    available = {a.strip() for a in args.available.split(",") if a.strip()}
    unknown = available - KNOWN_CAPABILITIES
    if unknown:
        print(f"Dépendances inconnues : {', '.join(sorted(unknown))}", file=sys.stderr)
        return 2
    suites = [s.strip() for s in args.suites.split(",") if s.strip()] or SUITES
    bad = [s for s in suites if s not in SUITES]
    if bad:
        print(f"Suites inconnues : {', '.join(bad)}", file=sys.stderr)
        return 2

    out_dir = os.path.abspath(args.out)
    os.makedirs(out_dir, exist_ok=True)
    env = dict(os.environ)
    if sys.platform != "win32" and "display" not in available:
        env.setdefault("QT_QPA_PLATFORM", "offscreen")

    results = []
    for suite in suites:
        missing = [r for r in REQUIREMENTS.get(suite, []) if r not in available]
        if missing:
            results.append({"suite": suite, "status": "NON EXÉCUTÉE", "missing": missing, "passed": 0, "fails": [], "skips": []})
            print(f"[NON EXÉCUTÉE] {suite} (absent : {', '.join(missing)})", flush=True)
            continue
        work = os.path.join(out_dir, "work", suite)
        shutil.rmtree(work, ignore_errors=True)
        res, log = run_suite(exe, suite, work, args.timeout, env)
        with open(os.path.join(out_dir, f"{suite}.log"), "w", encoding="utf-8") as f:
            f.write(log)
        results.append(res)
        print(f"[{res['status']}] {suite} : {res['passed']} réussi(s), {len(res['fails'])} échec(s), {len(res['skips'])} ignoré(s), "
              f"{res['seconds']} s", flush=True)

    executed = [r for r in results if r["status"] != "NON EXÉCUTÉE"]
    failed = [r for r in executed if r["status"] != "RÉUSSIE"]
    summary = {
        "date": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "executable": exe,
        "available": sorted(available),
        "suites_total": len(results),
        "suites_executed": len(executed),
        "suites_passed": len(executed) - len(failed),
        "suites_failed": len(failed),
        "suites_not_executed": len(results) - len(executed),
        "tests_passed": sum(r["passed"] for r in results),
        "tests_failed": sum(len(r["fails"]) for r in results),
        "tests_skipped": sum(len(r["skips"]) for r in results),
        "results": results,
    }
    with open(os.path.join(out_dir, "report.json"), "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)

    lines = ["# Tests TSA — rapport par suite", "",
             f"Dépendances présentes : {', '.join(sorted(available)) or 'aucune'}", "",
             f"- Suites exécutées : {summary['suites_executed']} / {summary['suites_total']} "
             f"(réussies {summary['suites_passed']}, en échec {summary['suites_failed']}, non exécutées {summary['suites_not_executed']})",
             f"- Tests réussis : {summary['tests_passed']} ; en échec : {summary['tests_failed']} ; ignorés : {summary['tests_skipped']}", "",
             "| Suite | Statut | Réussis | Échecs | Ignorés | Détail |", "|---|---|---:|---:|---:|---|"]
    for r in results:
        detail = ("absent : " + ", ".join(r["missing"])) if r.get("missing") else ("; ".join(r["fails"][:2]) if r["fails"] else "")
        lines.append(f"| {r['suite']} | {r['status']} | {r['passed']} | {len(r['fails'])} | {len(r['skips'])} | {detail.replace('|', '/')} |")
    skipped = [s for r in results for s in r["skips"]]
    if skipped:
        lines += ["", "## Tests ignorés par les suites", ""] + [f"- {s}" for s in skipped]
    with open(os.path.join(out_dir, "report.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print(f"\nSuites : {summary['suites_passed']} réussie(s), {summary['suites_failed']} en échec, "
          f"{summary['suites_not_executed']} non exécutée(s) — rapport : {os.path.join(out_dir, 'report.md')}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
