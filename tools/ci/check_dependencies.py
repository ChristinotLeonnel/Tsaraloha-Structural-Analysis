#!/usr/bin/env python3
"""Contrôle des dépendances tierces et de leurs licences (intégration continue). Ce n'est pas un avis juridique.

ERREUR (code de sortie 1) :
  - un composant tiers utilisé par la compilation (cmake/) n'a pas de ligne dans docs/THIRD_PARTY_LICENSES.md ;
  - OpenSees est inclus par défaut dans le paquet (TSA_PACKAGE_OPENSEES doit valoir OFF tant que les droits de
    redistribution ne sont pas vérifiés) ;
  - la licence du produit (LICENSE) est absente.
AVERTISSEMENT (signalé, non bloquant) :
  - textes de licence marqués « à ajouter » dans docs/THIRD_PARTY_LICENSES.md (à fournir avant toute diffusion) ;
  - Gmsh (GPL) lié dans le processus de TSA (conséquences : docs/THIRD_PARTY_LICENSES.md, section Gmsh).

Usage : python3 tools/ci/check_dependencies.py [racine du dépôt]
"""

import os
import re
import sys

# Composant → motifs qui prouvent son usage dans cmake/ et CMakeLists.txt, nom attendu dans le tableau des licences.
COMPONENTS = {
    "Qt": ([r"find_package\(Qt6", r"Qt6::"], "Qt"),
    "Open CASCADE Technology": ([r"OpenCASCADE", r"OCCT_LIBS"], "Open CASCADE"),
    "Gmsh": ([r"TSA::Gmsh"], "Gmsh"),
    "IfcOpenShell": ([r"IfcOpenShell"], "IfcOpenShell"),
    "OpenSees": ([r"OpenSees"], "OpenSees"),
    "Eigen": ([r"eigen"], "Eigen"),
    "Boost": ([r"boost"], "Boost"),
}


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def main():
    root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
    errors, warnings = [], []

    build_text = read(os.path.join(root, "CMakeLists.txt"))
    cmake_dir = os.path.join(root, "cmake")
    for name in sorted(os.listdir(cmake_dir)):
        if name.endswith(".cmake"):
            build_text += "\n" + read(os.path.join(cmake_dir, name))

    licenses_path = os.path.join(root, "docs", "THIRD_PARTY_LICENSES.md")
    if not os.path.exists(licenses_path):
        errors.append("docs/THIRD_PARTY_LICENSES.md absent")
        licenses = ""
    else:
        licenses = read(licenses_path)
    rows = [line for line in licenses.splitlines() if line.startswith("|")]

    for component, (patterns, row_name) in COMPONENTS.items():
        used = any(re.search(p, build_text, re.IGNORECASE) for p in patterns)
        if not used:
            continue
        matching = [r for r in rows if row_name.lower() in r.lower()]
        if not matching:
            errors.append(f"{component} utilisé par la compilation mais absent du tableau de docs/THIRD_PARTY_LICENSES.md")
        for r in matching:
            if "à ajouter" in r:
                warnings.append(f"{component} : texte de licence « à ajouter » (docs/THIRD_PARTY_LICENSES.md)")

    m = re.search(r'option\(TSA_PACKAGE_OPENSEES\s+"[^"]*"\s+(ON|OFF)\)', build_text)
    if not m:
        errors.append("option TSA_PACKAGE_OPENSEES introuvable (cmake/TSAProduct.cmake)")
    elif m.group(1) != "OFF":
        errors.append("TSA_PACKAGE_OPENSEES vaut ON par défaut : OpenSees ne doit pas être embarqué par défaut "
                      "(droits de redistribution non vérifiés, docs/THIRD_PARTY_LICENSES.md)")

    if re.search(r"target_link_libraries\(\$\{model\}\s+PUBLIC\s+TSA::Gmsh\)", build_text):
        warnings.append("Gmsh (GPL v2+) lié dans le processus de TSA (cible modèle) : voir docs/THIRD_PARTY_LICENSES.md, section Gmsh")

    if not os.path.exists(os.path.join(root, "LICENSE")):
        errors.append("licence du produit absente (LICENSE)")

    for w in sorted(set(warnings)):
        print("AVERT. :", w)
    for e in errors:
        print("ERREUR :", e)
    print(f"Dépendances et licences : {len(errors)} erreur(s), {len(set(warnings))} avertissement(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
