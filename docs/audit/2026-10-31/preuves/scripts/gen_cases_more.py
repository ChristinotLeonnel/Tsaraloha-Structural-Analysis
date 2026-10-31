import json, os, sys, glob, copy
root, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
# 1. RDM-11b en analyse LINÉAIRE avec geomTransf PDelta : le second ordre est-il appliqué, ou refusé ?
c = json.load(open(os.path.join(root, 'tests/benchmarks/rdm/RDM-11b.json'), encoding='utf-8'))
c['id'] = 'PDELTA-LIN'; c['title'] = 'P-Delta demandé en analyse statique LINÉAIRE'
c['analysis'].pop('type')
json.dump(c, open(os.path.join(out, 'PDELTA-LIN.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
# 2. Poutre à deux rotules (solive articulée) : travée isostatique, M max = qL²/8, moments d'extrémité nuls
E = 210000000.0
def pin(cid, title, load, Mmax, R, params):
    L = 6.0
    case = {"format": "tsa.benchmark/1", "id": cid, "title": title, "category": "rdm", "description": title,
            "sources": ["RDM : poutre isostatique sur deux appuis"], "hypotheses": ["rotules d'extrémité sur barre entre appuis encastrés"],
            "units": {"force": "kN", "length": "m", "stress": "kPa"}, "engines": ["opensees", "fem3d", "custom2d"],
            "parameters": {"L": L, "q": 12.0, "P": 30.0, "a": 2.0, "b": "L-a", **params},
            "model": {"materials": {"S": {"E": E, "nu": 0.3, "rho": 7850}}, "sections": {"R": {"shape": "rectangular", "b": 0.2, "h": 0.4}},
                      "nodes": [{"id": 1, "x": 0, "y": 0, "z": 0, "support": [1]*6}, {"id": 2, "x": L, "y": 0, "z": 0, "support": [1]*6}],
                      "members": [{"id": 1, "i": 1, "j": 2, "section": "R", "material": "S", "release_i": "my", "release_j": "my"}],
                      "cases": [{"id": 1, "name": "Q"}], "memberLoads": [load]},
            "analysis": {"case": 1, "selfWeight": False},
            "checks": [{"quantity": "member:1:M:start", "label": "M rotule i", "expected": 0, "unit": "kN·m", "tolerance": {"rel": 1e-6, "abs": 1e-6}},
                       {"quantity": "member:1:M:end", "label": "M rotule j", "expected": 0, "unit": "kN·m", "tolerance": {"rel": 1e-6, "abs": 1e-6}},
                       {"quantity": "member:1:M:absmax", "label": "M max travée", "expected": Mmax, "unit": "kN·m", "tolerance": {"rel": 2e-3, "abs": 1e-3}},
                       {"quantity": "reaction:1:fz", "label": "R_i", "expected": R, "unit": "kN", "tolerance": {"rel": 1e-6, "abs": 1e-6}}]}
    json.dump(case, open(os.path.join(out, cid + '.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
pin('PIN2-uniform', 'Deux rotules, charge uniforme (témoin)', {"member": 1, "type": "uniform", "case": 1, "q": -12.0, "direction": "globalZ"}, 'q*L^2/8', 'q*L/2', {})
pin('PIN2-trap', 'Deux rotules, trapézoïdale constante', {"member": 1, "type": "trapezoidal", "case": 1, "q1": -12.0, "q2": -12.0, "x1": 0.0, "x2": 1.0, "direction": "globalZ"}, 'q*L^2/8', 'q*L/2', {})
pin('PIN2-point', 'Deux rotules, ponctuelle excentrée', {"member": 1, "type": "point", "case": 1, "p": -30.0, "x": 2.0/6.0, "direction": "globalZ"}, 'P*a*b/L', 'P*b/L', {})
# 3. Barres subdivisées : mêmes références que les portiques aléatoires (famille A)
for f in sorted(glob.glob(os.path.join(sys.argv[3], 'RND-A*.json')))[:3]:
    c = json.load(open(f, encoding='utf-8'))
    c['id'] += '-sub3'; c['engines'] = ['opensees']; c['analysis']['subdivide'] = 3
    json.dump(c, open(os.path.join(out, c['id'] + '.json'), 'w', encoding='utf-8'), ensure_ascii=False)
