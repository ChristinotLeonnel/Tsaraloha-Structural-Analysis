#!/usr/bin/env python3
"""Lance TSA_Benchmark sans aucune fenêtre : parent CREATE_NO_WINDOW, erreurs système muettes, entrée fermée.
TSA_Benchmark n'écrit que dans --out et dans le dossier temporaire du système (lu : tools/benchmark/tsa_benchmark.cpp)."""
import ctypes, os, subprocess, sys

ctypes.windll.kernel32.SetErrorMode(0x8003)
build = r"D:\Tsaraloha Platforme\TSA\build-windows-validation"
cases, out, work, log = sys.argv[1:5]
os.makedirs(work, exist_ok=True)
env = dict(os.environ, PATH=build + os.pathsep + os.environ.get("PATH", ""))
p = subprocess.run([os.path.join(build, "TSA_Benchmark.exe"), "--cases", cases, "--out", out], cwd=work, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                   timeout=500, creationflags=subprocess.CREATE_NO_WINDOW)
open(log, "wb").write(p.stdout)
print("code", p.returncode)
