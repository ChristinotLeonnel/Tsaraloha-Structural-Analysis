#!/usr/bin/env python3
"""clang-tidy (LLVM de Visual Studio) sur une sélection de fichiers, en parallèle, sans modifier le dépôt."""
import os, re, subprocess, sys, concurrent.futures as cf, collections, json, time
repo, sp, listing, out = sys.argv[1:5]
jobs = int(sys.argv[5]) if len(sys.argv) > 5 else 6
rsp = open(os.path.join(sp, 'harness', 'compile.rsp'), encoding='utf-8').read()
defs = re.findall(r'(-D\S+|/D\S+)', rsp)
incs = re.findall(r'-I"([^"]+)"', rsp) + re.findall(r'-external:I"([^"]+)"', rsp) + re.findall(r'-external:I([^"\s]+)', rsp)
vc = r"C:\Program Files\Microsoft Visual Studio\18\Community\VC"
tidy = vc + r"\Tools\Llvm\x64\bin\clang-tidy.exe"
checks = ",".join(["-*", "bugprone-*", "-bugprone-easily-swappable-parameters", "-bugprone-narrowing-conversions", "-bugprone-implicit-widening-of-multiplication-result",
                   "-bugprone-branch-clone", "-bugprone-unchecked-optional-access", "-bugprone-macro-parentheses", "clang-analyzer-*", "-clang-analyzer-optin.*", "performance-unnecessary-copy-initialization",
                   "performance-for-range-copy", "performance-move-const-arg", "cppcoreguidelines-slicing", "concurrency-mt-unsafe", "cert-err58-cpp", "misc-unconventional-assign-operator"])
env = dict(os.environ)
extra = [f'--extra-arg={d.replace("/D", "-D", 1) if d.startswith("/D") else d}' for d in defs]
args_base = [tidy, '--quiet', f'--checks={checks}', '--header-filter=.*[/\\]TSA[/\\]src[/\\].*']
files = [l.strip() for l in open(listing, encoding='utf-8') if l.strip()]
os.makedirs(out, exist_ok=True)
def one(f):
    t = time.time()
    cmd = args_base + [os.path.join(repo, f), '--', '--driver-mode=cl', '/std:c++20', '/EHsc', '/utf-8', '/Zc:__cplusplus', '-Wno-everything']
    cmd += [d if d.startswith('-D') else d for d in defs]
    cmd += ['-I' + i for i in incs]
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=500, env=env, cwd=sp, stdin=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW)
        txt = p.stdout.decode('utf-8', 'replace')
    except subprocess.TimeoutExpired:
        txt = 'DÉLAI\n'
    open(os.path.join(out, f.replace('/', '__') + '.txt'), 'w', encoding='utf-8').write(txt)
    return f, time.time() - t, len(re.findall(r'warning:', txt)), len(re.findall(r'error:', txt))
with cf.ThreadPoolExecutor(jobs) as ex:
    for f, d, w, e in ex.map(one, files):
        print(f"{d:6.1f}s w={w:4d} e={e:3d} {f}", flush=True)
