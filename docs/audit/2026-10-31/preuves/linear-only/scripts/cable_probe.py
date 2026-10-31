#!/usr/bin/env python3
"""Câble précontraint résolu en un seul pas linéaire : la rigidité géométrique due à la précontrainte entre-t-elle dans
la matrice ? Corde horizontale de deux barres (2 × 5 m), nœud central libre en Z seulement, force transversale de 1 kN.

Rigidité de corde attendue si la rigidité géométrique est présente : k = 2·T0/a = 2 × 20 / 5 = 8 kN/m → uz = −0,125 m.
Sans elle, le DDL est sans raideur : matrice singulière. Essai masqué, dans un dossier hors dépôt.
Usage : cable_probe.py <dossier de travail>
"""
import ctypes
import os
import subprocess
import sys

ctypes.windll.kernel32.SetErrorMode(0x8003)
exe = r"D:\Tsaraloha Platforme\TSA\thirdparty\OpenSees\bin\OpenSees.exe"
work = sys.argv[1]
os.makedirs(work, exist_ok=True)

E, A, T0 = 2.0e8, 1.0e-4, 20.0   # kN/m², m², kN
eps0 = T0 / (E * A)
for element in ("corotTruss", "truss"):
    tcl = os.path.join(work, "cable_%s.tcl" % element)
    with open(tcl, "w") as f:
        f.write("model BasicBuilder -ndm 3 -ndf 3\n")
        f.write("node 1 0 0 0\nnode 2 5 0 0\nnode 3 10 0 0\n")
        f.write("fix 1 1 1 1\nfix 3 1 1 1\nfix 2 0 1 0\n")
        f.write("uniaxialMaterial Elastic 1 %r\n" % E)
        f.write("uniaxialMaterial InitStrain 2 1 %r\n" % eps0)
        f.write("element %s 1 1 2 %r 2\nelement %s 2 2 3 %r 2\n" % (element, A, element, A))
        f.write("timeSeries Linear 1\npattern Plain 1 1 { load 2 0.0 0.0 -1.0 }\n")
        f.write("constraints Transformation\nnumberer RCM\nsystem BandGeneral\ntest NormDispIncr 1e-06 50\n")
        f.write("algorithm Linear\nintegrator LoadControl 1.0\nanalysis Static\n")
        f.write("set ok [analyze 1]\n")
        f.write('puts "TSA_PROBE $ok ux=[nodeDisp 2 1] uz=[nodeDisp 2 3] N1=[eleResponse 1 axialForce] N2=[eleResponse 2 axialForce]"\n')
        f.write("reactions\n")
        f.write('puts "TSA_PROBE_R R1=[nodeReaction 1] R3=[nodeReaction 3]"\n')
        f.write("wipe\nexit\n")
    p = subprocess.run([exe, tcl], cwd=work, stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=60,
                       creationflags=subprocess.CREATE_NO_WINDOW)
    lines = [l for l in (p.stdout + p.stderr).splitlines() if "TSA_PROBE" in l or "WARNING" in l or "singular" in l.lower()]
    print("== %s (code %d)" % (element, p.returncode))
    for l in lines[:8]:
        print("  " + l.strip())
