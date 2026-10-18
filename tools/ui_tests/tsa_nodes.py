# Nœuds d'un projet .tsa (nom -> [x, y, z] en mètres, repère global), lus par TSA lui-même : tsa-ifc exporte le
# projet en IFC, chaque nœud y est un IfcStructuralPointConnection placé à l'origine dont la topologie est un
# IfcVertexPoint. Utilisé par les essais d'interface pour comparer les coordonnées réelles avant / après.
#
#   python tsa_nodes.py <tsa-ifc.exe> <projet.tsa> <sortie.json>

import json
import os
import re
import subprocess
import sys
import tempfile


def main():
    exe, project, out = (os.path.abspath(a) for a in sys.argv[1:4])
    with tempfile.TemporaryDirectory() as tmp:
        ifc = os.path.join(tmp, "nodes.ifc")
        r = subprocess.run([exe, "export", project, ifc], capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(ifc):
            print(f"tsa-ifc export a échoué ({r.returncode}) : {r.stdout}{r.stderr}", file=sys.stderr)
            return 1
        lines = {}
        with open(ifc, encoding="utf-8", errors="replace") as f:
            for line in f:
                m = re.match(r"#(\d+)=([A-Z0-9]+)\((.*)\);\s*$", line)
                if m:
                    lines[int(m.group(1))] = (m.group(2), m.group(3))

    nodes = {}
    for _, (kind, args) in lines.items():
        if kind != "IFCSTRUCTURALPOINTCONNECTION":
            continue
        name = re.findall(r"'([^']*)'", args)[1]
        shape = int(args.split(",")[6].strip().lstrip("#"))   # 7e argument : Representation
        rep = int(re.search(r"\(#(\d+)\)", lines[shape][1]).group(1))
        vertex = int(re.search(r"\(#(\d+)\)", lines[rep][1]).group(1))
        point = int(re.search(r"#(\d+)", lines[vertex][1]).group(1))
        coords = [float(v) for v in re.search(r"\(\(([^)]*)\)\)", "(" + lines[point][1] + ")").group(1).split(",")]
        nodes[name] = coords + [0.0] * (3 - len(coords))
    with open(out, "w", encoding="utf-8") as f:
        json.dump(nodes, f, indent=0, sort_keys=True)
    print(f"{len(nodes)} nœuds lus")
    return 0


if __name__ == "__main__":
    sys.exit(main())
