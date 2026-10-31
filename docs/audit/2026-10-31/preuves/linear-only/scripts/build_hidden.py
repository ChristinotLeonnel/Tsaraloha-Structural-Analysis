#!/usr/bin/env python3
"""Compile un dossier de build CMake/Ninja de TSA sans ouvrir de fenêtre (vcvars64 + cmake --build -j 4).

Usage : build_hidden.py <dossier de build> <journal> [cible ...]
"""
import os
import subprocess
import sys
import tempfile

build, log = sys.argv[1], sys.argv[2]
targets = sys.argv[3:]
vcvars = r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
target_args = " ".join("--target " + t for t in targets)
bat = os.path.join(tempfile.gettempdir(), "tsa_build_hidden.bat")
with open(bat, "w", encoding="ascii") as f:
    f.write("@echo off\r\n")
    f.write('call "%s" >nul\r\n' % vcvars)
    f.write('cmake --build "%s" %s -- -k 0 -j 4\r\n' % (build, target_args))
    f.write("exit /b %errorlevel%\r\n")
with open(log, "w", encoding="utf-8", errors="replace") as out:
    r = subprocess.run(["cmd", "/c", bat], stdin=subprocess.DEVNULL, stdout=out, stderr=subprocess.STDOUT,
                       creationflags=subprocess.CREATE_NO_WINDOW)
print("code", r.returncode)
sys.exit(r.returncode)
