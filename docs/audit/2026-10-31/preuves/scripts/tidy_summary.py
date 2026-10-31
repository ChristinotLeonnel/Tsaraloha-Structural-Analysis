import collections, glob, os, re, sys
d = sys.argv[1]
pat = re.compile(r"^(.*?):(\d+):(\d+): warning: (.*) \[([\w\.\-,]+)\]\s*$")
seen = {}
errs = delays = 0
for f in glob.glob(os.path.join(d, "*.txt")):
    txt = open(f, encoding="utf-8", errors="replace").read()
    errs += len(re.findall(r": error:", txt))
    delays += txt.startswith("DÉLAI")
    for line in txt.splitlines():
        m = pat.match(line)
        if m:
            p = os.path.normpath(m.group(1)).replace("\\", "/")
            i = p.find("/TSA/src/")
            if i < 0:
                continue
            key = (p[i + 5:], int(m.group(2)), m.group(5))
            seen[key] = m.group(4)
cnt = collections.Counter(k[2] for k in seen)
print("fichiers", len(glob.glob(os.path.join(d, "*.txt"))), "erreurs de compilation clang", errs, "délais", delays, "avertissements uniques dans src/", len(seen))
for c, n in cnt.most_common():
    print(f"{n:6d} {c}")
HOT = ("clang-analyzer", "use-after-move", "dangling", "slicing", "sizeof", "integer-division", "undefined-memory", "unhandled-self", "infinite-loop", "signed-char", "suspicious", "string-", "copy-constructor", "forward", "not-null", "unsafe", "throw", "parent-virtual", "misplaced", "assert", "mt-unsafe")
print("\n--- détail (hors exception-escape et incorrect-roundings)")
out = 0
for (p, l, c), msg in sorted(seen.items(), key=lambda kv: (kv[0][2], kv[0][0], kv[0][1])):
    if any(h in c for h in HOT):
        print(f"{c:44.44} {p}:{l} {msg[:96]}")
        out += 1
print("lignes", out)
