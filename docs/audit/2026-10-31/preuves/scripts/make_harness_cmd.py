#!/usr/bin/env python3
"""Génère harness/build.cmd : compile harness.cpp avec les drapeaux de TSA_Benchmark et lie sur les objets existants
(dossier build-windows-validation, lecture seule). Rien n'est écrit dans le dépôt ni dans le dossier de build."""
import os, re, sys

build, work = sys.argv[1], sys.argv[2]
text = open(os.path.join(build, "build.ninja"), encoding="utf-8", errors="replace").read()

# Bloc de liaison
m = re.search(r"^build TSA_Benchmark\.exe: (\S+) (.*?)\n((?:  .*\n)+)", text, re.M)
rule, inputs, block = m.group(1), m.group(2), m.group(3)
vars_ = dict(re.findall(r"^  (\w+) = (.*)$", block, re.M))
explicit = inputs.split(" | ")[0].split(" || ")[0]
# les chemins peuvent contenir des espaces échappés par $
objs = [o.replace("$ ", " ").replace("$:", ":") for o in re.findall(r"(?:[^ $]|\$ |\$:)+", explicit)]
objs = [o for o in objs if "tsa_benchmark.cpp" not in o]
print("règle", rule, "objets", len(objs), "variables", list(vars_))

# Bloc de compilation
c = re.search(r"^build (CMakeFiles\\TSA_Benchmark\.dir\\[^\n:]*tsa_benchmark\.cpp\.obj): (\S+) ([^\n]*)\n((?:  .*\n)+)", text, re.M)
cv = dict(re.findall(r"^  (\w+) = (.*)$", c.group(4), re.M))
print("compilation", c.group(1), list(cv))
for k in ("DEFINES", "INCLUDES", "FLAGS"):
    print(k, cv.get(k, "")[:300])

sp = os.path.abspath(work)
os.makedirs(sp, exist_ok=True)
rsp_c = os.path.join(sp, "compile.rsp")
flags = cv.get("FLAGS", "")
# le PCH de la cible n'est pas nécessaire : retiré pour ne dépendre d'aucun état du dossier de build
flags = re.sub(r"/Yu\S*|/Fp\S*|/FI\S*", "", flags.replace('"', ''))
flags = re.sub(r"\s+", " ", flags)
with open(rsp_c, "w", encoding="utf-8") as f:
    f.write(f'/nologo /c /utf-8 {flags} {cv.get("DEFINES","")} {cv.get("INCLUDES","")}\n')
rsp_l = os.path.join(sp, "link.rsp")
with open(rsp_l, "w", encoding="utf-8") as f:
    f.write(f'/nologo /OUT:"{os.path.join(sp, "harness.exe")}" {vars_.get("LINK_FLAGS","")} {vars_.get("LINK_PATH","")}\n')
    f.write('"' + os.path.join(sp, "harness.obj") + '"\n')
    for o in objs:
        f.write('"' + o + '"\n')
    f.write(vars_.get("LINK_LIBRARIES", "") + "\n")
vc = r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
with open(os.path.join(sp, "build.cmd"), "w", encoding="utf-8") as f:
    f.write("@echo off\r\nchcp 65001 >nul\r\n")
    f.write(f'call "{vc}" >nul\r\n')
    f.write(f'pushd "{build}"\r\n')
    f.write(f'cl @"{rsp_c}" /Fo"{os.path.join(sp, "harness.obj")}" "{os.path.join(sp, "harness.cpp")}"\r\n')
    f.write("if errorlevel 1 exit /b 1\r\n")
    f.write(f'link @"{rsp_l}"\r\n')
    f.write("set E=%errorlevel%\r\npopd\r\nexit /b %E%\r\n")
print("écrit", sp)
