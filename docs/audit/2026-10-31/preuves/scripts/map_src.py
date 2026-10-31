#!/usr/bin/env python3
"""Cartographie de src/ : lignes, fichiers, graphe d'inclusion entre modules, cycles (lecture seule)."""
import os, re, sys, collections, json

ROOT = sys.argv[1] if len(sys.argv) > 1 else '.'
SRC = os.path.join(ROOT, 'src')
EXT = ('.cpp', '.h', '.hpp', '.cc', '.c')

def norm(p):
    return p.replace(os.sep, '/').replace('\\', '/')

files = {}
for d, _, fs in os.walk(SRC):
    for f in fs:
        if f.endswith(EXT):
            p = os.path.join(d, f)
            rel = norm(os.path.relpath(p, SRC))
            with open(p, encoding='utf-8', errors='replace') as fh:
                files[rel] = fh.read()

def mod(rel):
    parts = rel.split('/')
    return parts[0] if len(parts) > 1 else '(racine)'

def submod(rel):
    parts = rel.split('/')
    return '/'.join(parts[:2]) if len(parts) > 2 else mod(rel)

loc = collections.Counter(); nf = collections.Counter()
sizes = []
for rel, txt in files.items():
    n = txt.count('\n') + 1
    loc[mod(rel)] += n; nf[mod(rel)] += 1
    sizes.append((n, rel))

print('TOTAL', sum(loc.values()), 'lignes', len(files), 'fichiers')
print('%-22s %8s %6s' % ('module', 'lignes', 'fich.'))
for k, v in loc.most_common():
    print('%-22s %8d %6d' % (k, v, nf[k]))

print('\n--- 50 plus gros fichiers')
for n, rel in sorted(sizes, reverse=True)[:50]:
    print(n, rel)
print('\n> 1000 lignes :', sum(1 for n, _ in sizes if n > 1000), ' > 2000 :', sum(1 for n, _ in sizes if n > 2000), ' > 4000 :', sum(1 for n, _ in sizes if n > 4000))

inc = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]', re.M)
basename = collections.defaultdict(list)
for rel in files:
    basename[os.path.basename(rel)].append(rel)

def resolve(from_rel, target):
    t = norm(target)
    # 1. relatif au fichier
    cand = norm(os.path.normpath(os.path.join(os.path.dirname(from_rel), t)))
    if cand in files:
        return cand
    # 2. relatif à src/
    if t in files:
        return t
    # 3. suffixe unique
    bn = os.path.basename(t)
    m = [r for r in basename.get(bn, []) if r.endswith('/' + t) or r == t]
    if len(m) == 1:
        return m[0]
    return None

edges = collections.Counter(); evidence = collections.defaultdict(list)
fan_in = collections.Counter(); fan_out = collections.Counter()
for rel, txt in files.items():
    for m in inc.finditer(txt):
        r = resolve(rel, m.group(1))
        if r:
            fan_in[r] += 1; fan_out[rel] += 1
            a, b = mod(rel), mod(r)
            if a != b:
                edges[(a, b)] += 1
                if len(evidence[(a, b)]) < 3:
                    evidence[(a, b)].append(rel + ' -> ' + r)

mods = sorted(set(mod(r) for r in files))
adj = {m: set() for m in mods}
for (a, b) in edges:
    adj[a].add(b)

# Tarjan
index = {}; low = {}; stack = []; on = set(); out = []; idx = [0]
sys.setrecursionlimit(10000)
def sc(v):
    index[v] = low[v] = idx[0]; idx[0] += 1; stack.append(v); on.add(v)
    for w in adj[v]:
        if w not in index:
            sc(w); low[v] = min(low[v], low[w])
        elif w in on:
            low[v] = min(low[v], index[w])
    if low[v] == index[v]:
        c = []
        while True:
            w = stack.pop(); on.discard(w); c.append(w)
            if w == v:
                break
        out.append(c)
for m_ in mods:
    if m_ not in index:
        sc(m_)
print('\n--- composantes fortement connexes (modules) de taille > 1 :')
for c in out:
    if len(c) > 1:
        print(len(c), sorted(c))

print('\n--- arêtes inter-modules (nb d\'inclusions)')
for (a, b), n in edges.most_common():
    print('%-20s -> %-20s %5d' % (a, b, n))

# Cycles d'inclusion au niveau fichier (en-têtes uniquement)
print('\n--- en-têtes : plus fort fan-in')
for r, n in fan_in.most_common(25):
    print(n, r)
print('\n--- fichiers : plus fort fan-out')
for r, n in fan_out.most_common(20):
    print(n, r)

json.dump({'edges': {f'{a}|{b}': n for (a, b), n in edges.items()}, 'evidence': {f'{a}|{b}': v for (a, b), v in evidence.items()}},
          open(sys.argv[2] if len(sys.argv) > 2 else 'map.json', 'w'), ensure_ascii=False, indent=1)
