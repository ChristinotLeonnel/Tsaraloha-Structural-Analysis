#!/usr/bin/env python3
"""Mesures « perf » du harnais (portiques 1 125, 8 330, 29 200 barres), sans fenêtre, dans un dossier hors dépôt.
Le harnais enchaîne une quatrième taille sans OpenSees (non mesurée avant) : le processus est arrêté après la
troisième ligne de mesures.
Usage : perf_run.py <harnais.exe> <dossier de travail> <journal> [nombre de tailles]"""
import ctypes
import os
import subprocess
import sys

ctypes.windll.kernel32.SetErrorMode(0x8003)
harness, work, log = sys.argv[1:4]
sizes = int(sys.argv[4]) if len(sys.argv) > 4 else 3
build = r"D:\Tsaraloha Platforme\TSA\build-windows-validation"
env = dict(os.environ, PATH=build + os.pathsep + os.environ.get("PATH", ""))
os.makedirs(work, exist_ok=True)
p = subprocess.Popen([harness, "perf", "1"], cwd=work, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
rows = 0
with open(log, "w", encoding="utf-8") as out:
    for raw in p.stdout:
        line = raw.decode("utf-8", "replace").rstrip()
        out.write(line + "\n")
        out.flush()
        print(line, flush=True)
        if line[:1].isdigit() and "|" in line:
            rows += 1
            if rows >= sizes:
                break
p.kill()
p.wait()
print("lignes", rows)
