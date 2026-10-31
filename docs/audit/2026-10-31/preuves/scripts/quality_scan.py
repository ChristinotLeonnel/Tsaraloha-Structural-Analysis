#!/usr/bin/env python3
"""Mesures de maintenabilité sur src/ (lecture seule) :
 - complexité cyclomatique approchée par fonction (compte de décisions dans le corps, analyse lexicale simple) ;
 - duplication de blocs (fenêtre glissante de 12 lignes normalisées) ;
 - fonctions membres déclarées dans un en-tête et jamais référencées ailleurs (candidats « code mort », à vérifier) ;
 - cas de casse d'inclusion (Windows tolérant, Linux non)."""
import collections, hashlib, os, re, sys

root = sys.argv[1]
src = os.path.join(root, "src")
files = {}
for d, _, fs in os.walk(src):
    for f in fs:
        if f.endswith((".cpp", ".h")):
            p = os.path.join(d, f)
            files[os.path.relpath(p, src).replace(os.sep, "/")] = open(p, encoding="utf-8", errors="replace").read()


def strip(txt):
    txt = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), txt, flags=re.S)
    txt = re.sub(r"//[^\n]*", "", txt)
    txt = re.sub(r'R"\w*\(.*?\)\w*"', '""', txt, flags=re.S)
    txt = re.sub(r'"(?:\\.|[^"\\\n])*"', '""', txt)
    txt = re.sub(r"'(?:\\.|[^'\\\n])'", "' '", txt)
    return txt


clean = {k: strip(v) for k, v in files.items()}

# --- complexité approchée
FUNC = re.compile(r"^(?:[\w:<>,&\*\s~\[\]]+?)\b([A-Za-z_][\w:~]*)\s*\(([^;{}]*)\)\s*(?:const)?\s*(?:noexcept)?\s*(?:override)?\s*(?::[^{;]*)?\{", re.M)
DEC = re.compile(r"\b(if|for|while|case|catch)\b|&&|\|\||\?")
funcs = []
for rel, txt in clean.items():
    if not rel.endswith(".cpp"):
        continue
    for m in FUNC.finditer(txt):
        name = m.group(1)
        if name in ("if", "for", "while", "switch", "catch", "return"):
            continue
        i, depth = m.end(), 1
        while i < len(txt) and depth:
            c = txt[i]
            depth += (c == "{") - (c == "}")
            i += 1
        body = txt[m.end():i]
        lines = body.count("\n") + 1
        funcs.append((1 + len(DEC.findall(body)), lines, name, rel, txt.count("\n", 0, m.start()) + 1))
funcs.sort(reverse=True)
print("fonctions analysées :", len(funcs))
print("CC > 50 :", sum(1 for f in funcs if f[0] > 50), " CC > 25 :", sum(1 for f in funcs if f[0] > 25), " corps > 200 lignes :", sum(1 for f in funcs if f[1] > 200), " corps > 400 lignes :", sum(1 for f in funcs if f[1] > 400))
print("--- 25 fonctions les plus complexes (CC approchée, lignes, nom, fichier:ligne)")
for cc, ln, n, r, l in funcs[:25]:
    print(f"{cc:5d} {ln:5d} {n:48.48} {r}:{l}")
print("--- 12 fonctions les plus longues")
for cc, ln, n, r, l in sorted(funcs, key=lambda f: -f[1])[:12]:
    print(f"{ln:5d} {cc:5d} {n:48.48} {r}:{l}")

# --- classes volumineuses
print("--- classes : nombre de méthodes déclarées (en-têtes)")
CLS = re.compile(r"^\s*class\s+(?:\w+_EXPORT\s+)?([A-Za-z_]\w*)(?:\s+final)?\s*(?::[^{;]*)?\{", re.M)
cls = []
for rel, txt in clean.items():
    if not rel.endswith(".h"):
        continue
    for m in CLS.finditer(txt):
        i, depth = m.end(), 1
        while i < len(txt) and depth:
            depth += (txt[i] == "{") - (txt[i] == "}")
            i += 1
        body = txt[m.end():i]
        methods = len(re.findall(r"^\s*(?:virtual\s+|static\s+|explicit\s+|inline\s+|Q_INVOKABLE\s+)*[\w:<>,&\*\s]+\b\w+\s*\([^;{]*\)\s*(?:const)?[^;{]*[;{]", body, re.M))
        members = len(re.findall(r"\bm_\w+\s*(?:=|;|\{)", body))
        cls.append((methods, members, m.group(1), rel))
for me, mb, n, r in sorted(cls, reverse=True)[:12]:
    print(f"{me:5d} méthodes {mb:4d} membres  {n:30.30} {r}")

# --- duplication
W = 12
index = collections.defaultdict(list)
for rel, txt in clean.items():
    if not rel.endswith(".cpp"):
        continue
    lines = [re.sub(r"\s+", " ", l).strip() for l in txt.split("\n")]
    keep = [(i + 1, l) for i, l in enumerate(lines) if len(l) > 3 and l not in ("{", "}", "};", "});")]
    for k in range(len(keep) - W):
        h = hashlib.md5("\n".join(l for _, l in keep[k:k + W]).encode()).hexdigest()
        index[h].append((rel, keep[k][0]))
dups = {h: v for h, v in index.items() if len(v) > 1}
dup_lines = len({(r, l) for v in dups.values() for r, l in v})
pairs = collections.Counter()
for v in dups.values():
    fs = sorted({r for r, _ in v})
    if len(fs) > 1:
        for a in range(len(fs)):
            for b in range(a + 1, len(fs)):
                pairs[(fs[a], fs[b])] += 1
print(f"--- duplication : {len(dups)} fenêtres de {W} lignes répétées, {dup_lines} positions concernées")
for (a, b), n in pairs.most_common(14):
    print(f"{n:5d} {a}  <->  {b}")

# --- casse des inclusions
names = {k.lower(): k for k in files}
inc = re.compile(r'#\s*include\s*"([^"]+)"')
bad = []
for rel, txt in files.items():
    for m in inc.finditer(txt):
        t = m.group(1).replace("\\", "/")
        cand = os.path.normpath(os.path.join(os.path.dirname(rel), t)).replace(os.sep, "/")
        if cand.lower() in names and names[cand.lower()] != cand:
            bad.append((rel, t))
print("--- inclusions à casse incorrecte :", len(bad), bad[:6])
print("--- antislash dans une inclusion :", sum(1 for t in files.values() for m in inc.finditer(t) if "\\" in m.group(1)))

# --- symboles jamais référencés (candidats)
alltext = "\n".join(clean.values())
try:
    tests = "\n".join(strip(open(os.path.join(root, "tests", f), encoding="utf-8", errors="replace").read()) for f in os.listdir(os.path.join(root, "tests")) if f.endswith((".cpp", ".h")))
except OSError:
    tests = ""
words = collections.Counter(re.findall(r"\b[A-Za-z_]\w{3,}\b", alltext))
twords = collections.Counter(re.findall(r"\b[A-Za-z_]\w{3,}\b", tests))
cand = []
for rel, txt in clean.items():
    if not rel.endswith(".h"):
        continue
    for m in re.finditer(r"^\s*(?:virtual\s+|static\s+|inline\s+)?[\w:<>,&\*\s]+?\b([a-z]\w{4,})\s*\([^;{)]*\)\s*(?:const)?\s*(?:noexcept)?\s*(?:;|\{)", txt, re.M):
        n = m.group(1)
        if "override" in m.group(0) or n.startswith(("on", "operator", "slot")):
            continue
        # déclaration + définition = 2 occurrences au plus
        if words[n] <= 2:
            cand.append((n, rel, twords[n]))
seen = set(); uniq = [c for c in cand if not (c[0] in seen or seen.add(c[0]))]
print(f"--- méthodes sans appelant dans src/ (≤ 2 occurrences : déclaration + définition) : {len(uniq)} ; dont non utilisées par les tests : {sum(1 for c in uniq if c[2] == 0)}")
for n, r, t in [c for c in uniq if c[2] == 0][:40]:
    print(f"   {n:42.42} {r}")
