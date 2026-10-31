#!/usr/bin/env python3
"""Arêtes d'inclusion à contre-courant de l'architecture documentée, cycles entre en-têtes, court-circuits UI."""
import json, os, re, sys, collections
sys.setrecursionlimit(20000)
root, mapjson = sys.argv[1], sys.argv[2]
src = os.path.join(root, "src")
ev = json.load(open(mapjson, encoding="utf-8"))
print("--- preuves des arêtes montantes (sens interdit par AGENTS.md / architecture.md)")
for k in ["Model|ExtensionSystem", "Model|UndoRedo", "UndoRedo|Commands", "Model|BIM", "Model|Grid", "Model|Annotation", "Grid|Viewer", "Viewer|UI", "Viewer|Commands", "Viewer|NDC", "Viewer|App",
          "Analysis|AI", "Analysis|Standards", "Standards|Analysis", "Analysis|App", "IO|App", "IO|Analysis", "IO|Standards", "App|UI", "Automation|AI", "AI|Automation", "Automation|Blueprint",
          "Blueprint|Automation", "Topology|Analysis", "BIM|Mesh", "Mesh|Analysis", "Coordinate|Model", "Diagnostics|Model", "NDC|App", "Reports|App", "Platform|App"]:
    if k in ev["evidence"]:
        print(f"{k.replace('|', ' -> '):28} n={ev['edges'][k]:3d}  {ev['evidence'][k][0]}")

files = {}
for d, _, fs in os.walk(src):
    for f in fs:
        if f.endswith((".h", ".cpp")):
            p = os.path.join(d, f)
            files[os.path.relpath(p, src).replace(os.sep, "/")] = open(p, encoding="utf-8", errors="replace").read()
inc = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]', re.M)
def resolve(rel, t):
    c = os.path.normpath(os.path.join(os.path.dirname(rel), t)).replace(os.sep, "/")
    return c if c in files else (t if t in files else None)
adj = collections.defaultdict(set)
for rel, txt in files.items():
    if rel.endswith(".h"):
        for t in inc.findall(txt):
            r = resolve(rel, t)
            if r and r.endswith(".h") and r != rel:
                adj[rel].add(r)
idx = {}; low = {}; st = []; on = set(); out = []; c = [0]
def sc(v):
    idx[v] = low[v] = c[0]; c[0] += 1; st.append(v); on.add(v)
    for w in adj[v]:
        if w not in idx:
            sc(w); low[v] = min(low[v], low[w])
        elif w in on:
            low[v] = min(low[v], idx[w])
    if low[v] == idx[v]:
        g = []
        while True:
            w = st.pop(); on.discard(w); g.append(w)
            if w == v:
                break
        if len(g) > 1:
            out.append(g)
for v in list(files):
    if v.endswith(".h") and v not in idx:
        sc(v)
print("\n--- cycles d'inclusion entre EN-TÊTES :", len(out))
for g in out:
    print(len(g), sorted(g)[:8])

print("\n--- court-circuits interface → solveur / fichier / OCCT")
def grep(pattern, where):
    r = re.compile(pattern, re.M)
    for rel, txt in sorted(files.items()):
        if rel.startswith(where):
            for m in r.finditer(txt):
                yield rel, m.group(0).strip()
hits = collections.Counter()
for rel, s in grep(r'^\s*#\s*include\s*"[^"]*(OpenSeesSolver|OpenSeesManager|OpenSeesAnalysisBuilder|OpenSeesResultsReader)\.h"', "UI/"):
    print("UI→solveur :", rel, s)
for rel, s in grep(r'^\s*#\s*include\s*"[^"]*(TSAFile|TSAPreviewGenerator)[^"]*\.h"', "UI/"):
    print("UI→fichier :", rel, s)
occ = collections.Counter(rel for rel, s in grep(r'^\s*#\s*include\s*<(AIS_|V3d_|BRep|gp_|TopoDS|Prs3d|Graphic3d|Geom_)[^>]*>', "UI/"))
print("UI→OCCT direct :", sum(occ.values()), "inclusions dans", len(occ), "fichiers ; principaux :", occ.most_common(8))
mut = collections.Counter(rel for rel, s in grep(r'pushUndoState\(', "UI/"))
print("pushUndoState dans UI :", sum(mut.values()))
opensees_ui = collections.Counter(rel for rel, s in grep(r'OpenSeesSolver|OpenSeesManager::', "UI/"))
print("mentions OpenSeesSolver/Manager dans UI :", opensees_ui.most_common(8))
qw = collections.Counter(rel for rel, s in grep(r'#\s*include\s*<(QMessageBox|QDialog|QWidget|QApplication)>', "Analysis/"))
print("widgets dans Analysis :", qw.most_common())
print("\n--- fuites de QtWidgets hors UI (couche modèle), en-tête direct ou Qt GUI")
for mod in ("Model/", "Analysis/", "IO/", "Coordinate/", "Commands/", "UndoRedo/", "BIM/Core/"):
    w = collections.Counter(s for rel, s in grep(r'#\s*include\s*<(QImage|QColor|QPainter|QPixmap|QFont|QIcon|QGuiApplication)>', mod))
    print(f"{mod:14}", dict(w))
