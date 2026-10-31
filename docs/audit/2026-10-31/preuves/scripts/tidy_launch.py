#!/usr/bin/env python3
"""Lance run_tidy.py dans l'environnement MSVC, sans aucune fenêtre."""
import os
import subprocess
import sys

vc = r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
sp, listing, out, jobs = sys.argv[1:5]
script = os.path.join(sp, "scripts", "run_tidy.py")
repo = r"D:\Tsaraloha Platforme\TSA"
batch = os.path.join(sp, "scripts", "tidy_env.cmd")
with open(batch, "w", encoding="utf-8") as f:
    f.write("@echo off\r\n")
    f.write(f'call "{vc}" >nul 2>nul\r\n')
    f.write(f'python "{script}" "{repo}" "{sp}" "{listing}" "{out}" {jobs}\r\n')
p = subprocess.run(["cmd", "/c", batch], stdin=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
sys.stdout.write(p.stdout.decode("utf-8", "replace"))
