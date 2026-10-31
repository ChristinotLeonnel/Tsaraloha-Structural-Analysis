#!/usr/bin/env python3
"""Solveur 3D de portiques, écrit pour l'audit, indépendant de TSA : méthode directe des rigidités, élément
d'Euler-Bernoulli exact, rotules par condensation statique, forces nodales cohérentes par intégration de Gauss.
Seule reprise de TSA : la DÉFINITION du repère local (documentée dans CoordinateTransformationService), nécessaire
pour exprimer les mêmes données ; tout le reste est dérivé de la théorie des poutres."""
import numpy as np

G_NODES, G_WEIGHTS = np.polynomial.legendre.leggauss(8)  # exact jusqu'au degré 15


def local_axes(p1, p2, beta_deg=0.0):
    x = p2 - p1
    L = np.linalg.norm(x)
    x = x / L
    ref = np.array([0.0, 1.0, 0.0]) if abs(x[2]) > 0.999 else np.array([0.0, 0.0, 1.0])
    y = np.cross(ref, x)
    y /= np.linalg.norm(y)
    z = np.cross(x, y)
    a = np.radians(beta_deg)
    y2 = y * np.cos(a) + z * np.sin(a)
    z2 = z * np.cos(a) - y * np.sin(a)
    return L, np.vstack([x, y2, z2])


def k_local(E, G, A, J, Iy, Iz, L):
    k = np.zeros((12, 12))
    ea, gj = E * A / L, G * J / L
    for (a, b, v) in [(0, 0, ea), (0, 6, -ea), (6, 6, ea), (3, 3, gj), (3, 9, -gj), (9, 9, gj)]:
        k[a, b] = k[b, a] = v
    c = E * Iz / L**3
    m = np.array([[12, 6 * L, -12, 6 * L], [6 * L, 4 * L**2, -6 * L, 2 * L**2], [-12, -6 * L, 12, -6 * L], [6 * L, 2 * L**2, -6 * L, 4 * L**2]])
    idx = [1, 5, 7, 11]
    k[np.ix_(idx, idx)] += c * m
    c = E * Iy / L**3
    m = np.array([[12, -6 * L, -12, -6 * L], [-6 * L, 4 * L**2, 6 * L, 2 * L**2], [-12, 6 * L, 12, 6 * L], [-6 * L, 2 * L**2, 6 * L, 4 * L**2]])
    idx = [2, 4, 8, 10]
    k[np.ix_(idx, idx)] += c * m
    return k


def shape_force(x, L, p):
    """Forces nodales équivalentes d'une force p=(px,py,pz) en x : f = Nᵀ p (travail virtuel)."""
    s = x / L
    h1 = 1 - 3 * s**2 + 2 * s**3
    h2 = L * (s - 2 * s**2 + s**3)
    h3 = 3 * s**2 - 2 * s**3
    h4 = L * (-(s**2) + s**3)
    f = np.zeros(12)
    f[0], f[6] = (1 - s) * p[0], s * p[0]
    f[1], f[5], f[7], f[11] = h1 * p[1], h2 * p[1], h3 * p[1], h4 * p[1]
    f[2], f[4], f[8], f[10] = h1 * p[2], -h2 * p[2], h3 * p[2], -h4 * p[2]  # θy = −dw/dx
    return f


def equivalent(load, L):
    """load : dict(kind, q1, q2 (vecteurs locaux), a, b) ou point(p, a)."""
    if load["kind"] == "point":
        return shape_force(load["a"], L, load["p"])
    a, b = load["a"], load["b"]
    f = np.zeros(12)
    for t, w in zip(G_NODES, G_WEIGHTS):
        xi = 0.5 * (t + 1)
        x = a + (b - a) * xi
        q = load["q1"] + (load["q2"] - load["q1"]) * xi
        f += shape_force(x, L, q) * w * 0.5 * (b - a)
    return f


class Frame:
    def __init__(self):
        self.nodes = {}       # id -> xyz
        self.fix = {}         # id -> 6 booléens
        self.spring = {}      # id -> 6 raideurs
        self.members = {}     # id -> dict
        self.nodal = {}       # id -> 6 forces

    def solve(self):
        ids = sorted(self.nodes)
        pos = {n: i for i, n in enumerate(ids)}
        ndof = 6 * len(ids)
        K, F = np.zeros((ndof, ndof)), np.zeros(ndof)
        data = {}
        for mid, m in self.members.items():
            p1, p2 = np.array(self.nodes[m["i"]], float), np.array(self.nodes[m["j"]], float)
            L, R = local_axes(p1, p2, m.get("beta", 0.0))
            T = np.zeros((12, 12))
            for b in range(4):
                T[3 * b:3 * b + 3, 3 * b:3 * b + 3] = R
            k = k_local(m["E"], m["G"], m["A"], m["J"], m["Iy"], m["Iz"], L)
            fe = np.zeros(12)
            for l in m.get("loads", []):
                fe += equivalent(l, L)
            rel = list(m.get("released", []))
            kept = [i for i in range(12) if i not in rel]
            if rel:
                kbb = k[np.ix_(rel, rel)]
                kab = k[np.ix_(kept, rel)]
                kc = np.zeros((12, 12))
                kc[np.ix_(kept, kept)] = k[np.ix_(kept, kept)] - kab @ np.linalg.solve(kbb, kab.T)
                fc = np.zeros(12)
                fc[kept] = fe[kept] - kab @ np.linalg.solve(kbb, fe[rel])
            else:
                kc, fc = k, fe
            dofs = [6 * pos[m["i"]] + d for d in range(6)] + [6 * pos[m["j"]] + d for d in range(6)]
            K[np.ix_(dofs, dofs)] += T.T @ kc @ T
            F[dofs] += T.T @ fc
            data[mid] = dict(T=T, k=k, fe=fe, rel=rel, kept=kept, dofs=dofs, L=L)
        for n, f in self.nodal.items():
            F[6 * pos[n]:6 * pos[n] + 6] += np.array(f, float)
        free = np.ones(ndof, bool)
        for n, fx in self.fix.items():
            for d, v in enumerate(fx):
                if v:
                    free[6 * pos[n] + d] = False
        for n, sp in self.spring.items():
            for d, v in enumerate(sp):
                if v and free[6 * pos[n] + d]:
                    K[6 * pos[n] + d, 6 * pos[n] + d] += v
        fi = np.where(free)[0]
        Kff = K[np.ix_(fi, fi)]
        u = np.zeros(ndof)
        cond = 1.0
        if fi.size:
            # équilibrage diagonal : translations et rotations ont des échelles différentes
            d = np.sqrt(np.diag(Kff))
            S = np.diag(1 / d)
            cond = np.linalg.cond(S @ Kff @ S)
            u[fi] = np.linalg.solve(Kff, F[fi])
        R = K @ u - F
        res = dict(u={n: u[6 * pos[n]:6 * pos[n] + 6] for n in ids}, r={}, m={}, cond=cond)
        for n in ids:
            r = R[6 * pos[n]:6 * pos[n] + 6].copy()
            for dd in range(6):
                if free[6 * pos[n] + dd]:
                    r[dd] = 0.0
            res["r"][n] = r
        for n, sp in self.spring.items():
            for dd, v in enumerate(sp):
                if v and free[6 * pos[n] + dd]:
                    res["r"][n][dd] += -v * u[6 * pos[n] + dd]
        for mid, q in data.items():
            d = q["T"] @ u[q["dofs"]]
            rel, kept, k, fe = q["rel"], q["kept"], q["k"], q["fe"]
            if rel:
                kbb = k[np.ix_(rel, rel)]
                d[rel] = np.linalg.solve(kbb, fe[rel] - k[np.ix_(rel, kept)] @ d[kept])
            f = k @ d - fe
            for i in rel:
                f[i] = 0.0
            res["m"][mid] = f
        return res
