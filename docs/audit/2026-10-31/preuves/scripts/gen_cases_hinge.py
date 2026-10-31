#!/usr/bin/env python3
"""Cas d'audit : rotule + charges sur barre. Références analytiques indépendantes (RDM classique, formules ci-dessous).

Barre encastrée en j, ARTICULÉE (rotule d'élément) en i, longueur L, flexion dans le plan x-z (chargement selon -Z).
Formules de la poutre appuyée-encastrée (Roark, table 8.1 ; rederivables par la méthode des forces) :
  uniforme q                : R_i = 3qL/8,   M_j = qL²/8
  triangle 0 (i) → q (j)    : R_i = qL/10,   M_j = qL²/15
  triangle q (i) → 0 (j)    : R_i = 11qL/40, M_j = 7qL²/120
  ponctuelle P en a (b=L-a) : R_i = P b²(2L+a)/(2L³), M_j = P a b (L+a)/(2L²)
Poutre à rotule (Gerber) : console 1-2 de longueur L1, travée 2-3 de longueur L2 articulée en 2, appui simple en 3 :
  charge q uniforme sur 2-3 : R_3 = qL2/2, V_1 = qL2/2, M_1 = qL2/2·L1 (isostatique : indépendant des rigidités)
"""
import json, os, sys

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
E = 210000000.0

def base(cid, title, desc, params, nodes, members, loads, checks, engines=("opensees", "fem3d", "custom2d")):
    c = {
        "format": "tsa.benchmark/1", "id": cid, "title": title, "category": "rdm", "description": desc,
        "sources": ["W. Young, R. Budynas — Roark's Formulas for Stress and Strain, poutres hyperstatiques"],
        "hypotheses": ["Euler-Bernoulli, petits déplacements"],
        "units": {"force": "kN", "length": "m", "stress": "kPa"},
        "engines": list(engines),
        "parameters": {"E": E, "nu": 0.3, **params},
        "model": {
            "materials": {"S": {"E": E, "nu": 0.3, "rho": 7850}},
            "sections": {"R": {"shape": "rectangular", "b": 0.2, "h": 0.4}},
            "nodes": nodes, "members": members,
            "cases": [{"id": 1, "name": "Q"}],
            "memberLoads": loads,
        },
        "analysis": {"case": 1, "selfWeight": False},
        "checks": checks,
    }
    with open(os.path.join(out, cid + ".json"), "w", encoding="utf-8") as f:
        json.dump(c, f, ensure_ascii=False, indent=1)

FIX = [1, 1, 1, 1, 1, 1]
PIN_FREE_RY = [1, 1, 1, 1, 0, 1]
tol = {"rel": 1e-6, "abs": 1e-9}

def propped(cid, title, load, Ri, Mj, extra=None, L=6.0, q=12.0):
    nodes = [{"id": 1, "x": 0, "y": 0, "z": 0, "support": FIX}, {"id": 2, "x": L, "y": 0, "z": 0, "support": FIX}]
    members = [{"id": 1, "i": 1, "j": 2, "section": "R", "material": "S", "release_i": "my"}]
    checks = [
        {"quantity": "reaction:1:fz", "label": "R_i", "expected": Ri, "unit": "kN", "tolerance": tol},
        {"quantity": "reaction:2:fz", "label": "R_j", "expected": f"Rtot-({Ri})", "unit": "kN", "tolerance": tol},
        {"quantity": "member:1:M:start", "label": "moment nul à la rotule", "expected": 0, "unit": "kN·m", "tolerance": {"rel": 1e-6, "abs": 1e-6}},
        {"quantity": "member:1:M:end", "label": "|M_j|", "expected": Mj, "unit": "kN·m", "tolerance": tol},
    ]
    params = {"L": L, "q": q, "P": 30.0, "a": 2.0, "b": "L-a"}
    params.update(extra or {})
    base(cid, title, title, params, nodes, members, [load], checks)

# Témoins : voies eleLoad d'OpenSees
propped("HINGE-01-uniform", "Rotule + charge uniforme (beamUniform)",
        {"member": 1, "type": "uniform", "case": 1, "q": -12.0, "direction": "globalZ"},
        "3*q*L/8", "q*L^2/8", {"Rtot": "q*L"})
propped("HINGE-02-point", "Rotule + force ponctuelle (beamPoint)",
        {"member": 1, "type": "point", "case": 1, "p": -30.0, "x": 2.0 / 6.0, "direction": "globalZ"},
        "P*b^2*(2*L+a)/(2*L^3)", "P*a*b*(L+a)/(2*L^2)", {"Rtot": "P"})
# Voie forces nodales équivalentes (Kind::Linear)
propped("HINGE-03-trap-constant", "Rotule + trapézoïdale constante (même charge que HINGE-01)",
        {"member": 1, "type": "trapezoidal", "case": 1, "q1": -12.0, "q2": -12.0, "x1": 0.0, "x2": 1.0, "direction": "globalZ"},
        "3*q*L/8", "q*L^2/8", {"Rtot": "q*L"})
propped("HINGE-04-tri-up", "Rotule + triangle 0→q",
        {"member": 1, "type": "trapezoidal", "case": 1, "q1": 0.0, "q2": -12.0, "x1": 0.0, "x2": 1.0, "direction": "globalZ"},
        "q*L/10", "q*L^2/15", {"Rtot": "q*L/2"})
propped("HINGE-05-tri-down", "Rotule + triangle q→0",
        {"member": 1, "type": "trapezoidal", "case": 1, "q1": -12.0, "q2": 0.0, "x1": 0.0, "x2": 1.0, "direction": "globalZ"},
        "11*q*L/40", "7*q*L^2/120", {"Rtot": "q*L/2"})

# Gerber : rotation du nœud 2 libre (nœud réel, charge transmise aux nœuds)
def gerber(cid, title, load, engines=("opensees", "fem3d", "custom2d")):
    L1, L2, q = 3.0, 6.0, 12.0
    nodes = [{"id": 1, "x": 0, "y": 0, "z": 0, "support": FIX},
             {"id": 2, "x": L1, "y": 0, "z": 0, "support": [0, 1, 0, 1, 0, 1]},
             {"id": 3, "x": L1 + L2, "y": 0, "z": 0, "support": [1, 1, 1, 1, 0, 1]}]
    members = [{"id": 1, "i": 1, "j": 2, "section": "R", "material": "S"},
               {"id": 2, "i": 2, "j": 3, "section": "R", "material": "S", "release_i": "my"}]
    checks = [
        {"quantity": "reaction:3:fz", "label": "R_3 = qL2/2", "expected": "q*L2/2", "unit": "kN", "tolerance": tol},
        {"quantity": "reaction:1:fz", "label": "V_1 = qL2/2", "expected": "q*L2/2", "unit": "kN", "tolerance": tol},
        {"quantity": "reaction:1:my", "label": "|M_1| = qL2 L1 / 2 (signe : main droite)", "expected": "-q*L2/2*L1", "unit": "kN·m", "tolerance": tol},
        {"quantity": "member:2:M:start", "label": "moment nul à la rotule", "expected": 0, "unit": "kN·m", "tolerance": {"rel": 1e-6, "abs": 1e-6}},
        {"quantity": "member:2:M:absmax", "label": "M travée = qL2²/8", "expected": "q*L2^2/8", "unit": "kN·m", "tolerance": {"rel": 1e-3, "abs": 1e-3}},
    ]
    base(cid, title, title, {"L1": L1, "L2": L2, "q": q}, nodes, members, [load], checks, engines)

gerber("GERBER-01-uniform", "Poutre à rotule, charge uniforme (témoin)",
       {"member": 2, "type": "uniform", "case": 1, "q": -12.0, "direction": "globalZ"})
gerber("GERBER-02-trap", "Poutre à rotule, trapézoïdale constante (voie nodale)",
       {"member": 2, "type": "trapezoidal", "case": 1, "q1": -12.0, "q2": -12.0, "x1": 0.0, "x2": 1.0, "direction": "globalZ"})
print("cas écrits dans", out)
