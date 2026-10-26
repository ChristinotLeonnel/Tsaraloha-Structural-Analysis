#!/usr/bin/env python3
"""Contrôle : aucune initialisation statique dynamique dans le noyau TSA_Core (TSA Core, étape 1).

<P>_Core est une bibliothèque STATIQUE : l'éditeur de liens n'en garde que les objets référencés, si bien qu'un
enregistrement par initialisation statique (objet global à constructeur, Q_COREAPP_STARTUP_FUNCTION…) y serait
silencieusement perdu. Les enregistrements se font dans les couches supérieures (bibliothèques OBJECT), comme le
fournisseur des catalogues installés (ExtensionSystem/LibraryRegistry.cpp).

Détection : symbole d'initialisation par unité de traduction de GCC / Clang (_GLOBAL__sub_I_*), lu par nm.
Usage : python tools/check_core_static_init.py <nm> <libTSA_Core.a>   (code de sortie 1 si une unité en contient)
"""
import subprocess
import sys


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    nm, library = sys.argv[1], sys.argv[2]
    result = subprocess.run([nm, "-A", library], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"nm a échoué ({result.returncode}) : {result.stderr.strip()}")
        return 2
    members = set()
    offenders = set()
    for line in result.stdout.splitlines():
        # « libTSA_Core.a:Model.cpp.o:0000000000000000 T symbole »
        parts = line.split(":")
        if len(parts) < 3:
            continue
        members.add(parts[1])
        if "_GLOBAL__sub_I_" in line:
            offenders.add(parts[1])
    if not members:
        print(f"aucun objet lu dans {library}")
        return 2
    for member in sorted(offenders):
        print(f"INITIALISATION STATIQUE interdite dans le noyau : {member}")
    print(f"check_core_static_init : {len(members)} objet(s), {len(offenders)} avec initialisation statique")
    return 1 if offenders else 0


if __name__ == "__main__":
    sys.exit(main())
