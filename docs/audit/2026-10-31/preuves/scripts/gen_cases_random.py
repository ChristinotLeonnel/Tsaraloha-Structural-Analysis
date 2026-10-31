#!/usr/bin/env python3
"""Génère des portiques 3D aléatoires (graine fixe), les résout avec frame3d.py et écrit des cas tsa.benchmark/1 dont
les valeurs attendues sont celles du solveur indépendant.
  famille A : barres inclinées, sections tournées, charges nodales et sur barre, ressorts — SANS rotule
  famille B : avec rotules, charge uniforme seule sur les barres à rotule
  famille C : avec rotules, charges ponctuelles et trapézoïdales sur les barres à rotule
Usage : gen_cases_random.py <dossier> [nb_par_famille]"""
import json, math, os, random, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from frame3d import Frame, local_axes

out = sys.argv[1]
count = int(sys.argv[2]) if len(sys.argv) > 2 else 6
os.makedirs(out, exist_ok=True)
E = 210000000.0; NU = 0.3; G = E / (2 * (1 + NU))


def rect(b, h):
    a_, b_ = max(b, h), min(b, h)
    return dict(b=b, h=h, A=b * h, Iy=b * h**3 / 12, Iz=h * b**3 / 12,
                J=a_ * b_**3 * (1 / 3 - 0.21 * (b_ / a_) * (1 - (b_ / a_)**4 / 12)))


def make(seed, family):
    rng = random.Random(seed)
    nx, ny, lv = rng.choice([2, 3]), rng.choice([1, 2]), rng.choice([2, 3])
    f = Frame()
    nodes, ids = [], {}
    nid = 0
    for k in range(lv + 1):
        for j in range(ny + 1):
            for i in range(nx + 1):
                nid += 1
                x = 5.0 * i + rng.uniform(-0.6, 0.6)
                y = 4.0 * j + rng.uniform(-0.6, 0.6)
                z = 3.2 * k + (0 if k == 0 else rng.uniform(-0.3, 0.3))
                f.nodes[nid] = (round(x, 4), round(y, 4), round(z, 4))
                ids[(i, j, k)] = nid
                sup = None
                if k == 0:
                    sup = [1, 1, 1, 1, 1, 1] if rng.random() < 0.7 else [1, 1, 1, 0, 0, 0]
                    f.fix[nid] = sup
                nodes.append(dict(id=nid, x=f.nodes[nid][0], y=f.nodes[nid][1], z=f.nodes[nid][2], **({"support": sup} if sup else {})))
    # ressort sur deux nœuds courants
    for nd in rng.sample([n for n in f.nodes if n not in f.fix], 2):
        st = [0, 0, 0, 0, 0, 0]
        k_ = [0.0] * 6
        d = rng.randrange(6)
        k_[d] = round(rng.uniform(2e3, 5e4), 1)
        st[d] = 0
        f.spring[nd] = k_
    sections, members, mlist, loads = {}, [], {}, []
    mid = 0
    def add(a, b, kind, role):
        nonlocal mid
        mid += 1
        s = rect(round(rng.uniform(0.15, 0.4), 3), round(rng.uniform(0.25, 0.7), 3))
        name = f"R{mid}"
        sections[name] = {"shape": "rectangular", "b": s["b"], "h": s["h"]}
        beta = rng.choice([0.0, 0.0, 30.0, 90.0, -17.0, 45.0])
        m = dict(i=a, j=b, E=E, G=G, A=s["A"], J=s["J"], Iy=s["Iy"], Iz=s["Iz"], beta=beta, loads=[], released=[])
        jm = dict(id=mid, type=kind, i=a, j=b, section=name, material="S", rotation=beta)
        if role == "beam" and family in "BC" and rng.random() < 0.5:
            ri = rng.choice(["my", "mz", "mymz", ""])
            rj = rng.choice(["my", "mz", "mymz", ""])
            if "my" in ri: m["released"].append(4)
            if "mz" in ri: m["released"].append(5)
            if "my" in rj: m["released"].append(10)
            if "mz" in rj: m["released"].append(11)
            jm["release_i"], jm["release_j"] = ri, rj
        f.members[mid] = m
        members.append(jm)
        return mid
    for k in range(1, lv + 1):
        for j in range(ny + 1):
            for i in range(nx + 1):
                add(ids[(i, j, k - 1)], ids[(i, j, k)], "column", "column")
                if i < nx: add(ids[(i, j, k)], ids[(i + 1, j, k)], "beam", "beam")
                if j < ny: add(ids[(i, j, k)], ids[(i, j + 1, k)], "beam", "beam")
    # diagonales
    for _ in range(2):
        k = rng.randrange(1, lv + 1)
        i, j = rng.randrange(nx), rng.randrange(ny + 1)
        add(ids[(i, j, k - 1)], ids[(i + 1, j, k)], "beam", "diag")
    # charges nodales
    nodal = []
    for nd in rng.sample([n for n in f.nodes if n not in f.fix], 3):
        v = [round(rng.uniform(-50, 50), 2) for _ in range(3)] + [round(rng.uniform(-20, 20), 2) for _ in range(3)]
        f.nodal[nd] = v
        nodal.append(dict(node=nd, case=1, fx=v[0], fy=v[1], fz=v[2], mx=v[3], my=v[4], mz=v[5]))
    # charges sur barre
    for mm_id, m in f.members.items():
        L, R = local_axes(np.array(f.nodes[m["i"]], float), np.array(f.nodes[m["j"]], float), m["beta"])
        hinged = bool(m["released"])
        kinds = ["uniform", "trapezoidal", "partial", "point", None]
        if family == "B" and hinged:
            kinds = ["uniform", None]
        kind = rng.choice(kinds)
        if not kind:
            continue
        dname = rng.choice(["globalX", "globalY", "globalZ", "localX", "localY", "localZ"])
        unit = {"globalX": R @ [1, 0, 0], "globalY": R @ [0, 1, 0], "globalZ": R @ [0, 0, 1],
                "localX": np.array([1.0, 0, 0]), "localY": np.array([0, 1.0, 0]), "localZ": np.array([0, 0, 1.0])}[dname]
        q1, q2 = round(rng.uniform(-30, 30), 2), round(rng.uniform(-30, 30), 2)
        if kind == "uniform":
            m["loads"].append(dict(kind="dist", q1=unit * q1, q2=unit * q1, a=0.0, b=L))
            loads.append(dict(member=mm_id, type="uniform", case=1, q=q1, direction=dname))
        elif kind == "trapezoidal":
            m["loads"].append(dict(kind="dist", q1=unit * q1, q2=unit * q2, a=0.0, b=L))
            loads.append(dict(member=mm_id, type="trapezoidal", case=1, q1=q1, q2=q2, x1=0.0, x2=1.0, direction=dname))
        elif kind == "partial":
            a = round(rng.uniform(0.1, 0.4), 3); b = round(rng.uniform(0.55, 0.9), 3)
            m["loads"].append(dict(kind="dist", q1=unit * q1, q2=unit * q2, a=a * L, b=b * L))
            loads.append(dict(member=mm_id, type="trapezoidal", case=1, q1=q1, q2=q2, x1=a, x2=b, direction=dname))
        else:
            a = round(rng.uniform(0.15, 0.85), 3)
            m["loads"].append(dict(kind="point", p=unit * q1 * 2, a=a * L))
            loads.append(dict(member=mm_id, type="point", case=1, p=q1 * 2, x=a, direction=dname))
    r = f.solve()
    cid = f"RND-{family}{seed:03d}"
    checks = []
    tolD = {"rel": 2e-5, "abs": 2e-9}; tolR = {"rel": 2e-5, "abs": 2e-9}; tolF = {"rel": 2e-5, "abs": 1e-5}
    for n, u in r["u"].items():
        for name, v in zip(["ux", "uy", "uz", "rx", "ry", "rz"], u):
            checks.append(dict(quantity=f"node:{n}:{name}", label=f"{name} N{n}", expected=float(v), unit="m", tolerance=tolD))
    for n, rr in r["r"].items():
        if n in f.fix or n in f.spring:
            for name, v in zip(["fx", "fy", "fz", "mx", "my", "mz"], rr):
                checks.append(dict(quantity=f"reaction:{n}:{name}", label=f"{name} N{n}", expected=float(v), unit="kN", tolerance=tolF))
    for mm_id, fl in r["m"].items():
        checks.append(dict(quantity=f"member:{mm_id}:M:start", label=f"|M| i B{mm_id}", expected=float(math.hypot(fl[4], fl[5])), unit="kN·m", tolerance=tolF))
        checks.append(dict(quantity=f"member:{mm_id}:M:end", label=f"|M| j B{mm_id}", expected=float(math.hypot(fl[10], fl[11])), unit="kN·m", tolerance=tolF))
        checks.append(dict(quantity=f"member:{mm_id}:N:start", label=f"N i B{mm_id}", expected=float(-fl[0]), unit="kN", tolerance=tolF))
    jn = []
    for nd in nodes:
        jn.append(nd)
        if nd["id"] in f.spring:
            nd["support"] = [0, 0, 0, 0, 0, 0]
            nd["springs"] = f.spring[nd["id"]]
    case = {
        "format": "tsa.benchmark/1", "id": cid, "title": f"Portique 3D aléatoire {family}{seed}", "category": "fea",
        "description": "Référence : solveur indépendant frame3d.py (audit).",
        "sources": ["Solveur indépendant écrit pour l'audit (Euler-Bernoulli, méthode directe des rigidités)"],
        "hypotheses": ["élasticité linéaire", f"conditionnement équilibré {r['cond']:.2e}"],
        "units": {"force": "kN", "length": "m", "stress": "kPa"},
        "engines": ["opensees", "fem3d"],
        "parameters": {},
        "model": {"materials": {"S": {"E": E, "nu": NU, "rho": 7850}}, "sections": sections, "nodes": jn, "members": members,
                  "cases": [{"id": 1, "name": "Q"}], "nodalLoads": nodal, "memberLoads": loads},
        "analysis": {"case": 1, "selfWeight": False},
        "checks": checks,
    }
    with open(os.path.join(out, cid + ".json"), "w", encoding="utf-8") as fh:
        json.dump(case, fh, ensure_ascii=False, indent=0)
    return cid, len(checks), r["cond"], len(f.members)


for fam in "ABC":
    for s in range(1, count + 1):
        print(make(s * 7 + ord(fam), fam))
