#!/usr/bin/env python3
"""OpenSees se bloque-t-il quand son entrée standard reste ouverte ? Essai masqué, délai de 25 s."""
import ctypes, os, subprocess, sys, tempfile, time

ctypes.windll.kernel32.SetErrorMode(0x8003)
exe = r"D:\Tsaraloha Platforme\TSA\thirdparty\OpenSees\bin\OpenSees.exe"
work = sys.argv[1]
os.makedirs(work, exist_ok=True)
tcl = os.path.join(work, "essai.tcl")
open(tcl, "w").write("model BasicBuilder -ndm 2 -ndf 3\nnode 1 0 0\nwipe\nputs TSA_AUDIT_OK\nexit\n")
for label, stdin in (("entrée fermée (DEVNULL)", subprocess.DEVNULL), ("entrée ouverte (tube jamais écrit)", subprocess.PIPE)):
    t = time.monotonic()
    p = subprocess.Popen([exe, tcl], cwd=work, stdin=stdin, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        out, _ = p.communicate(timeout=25) if stdin == subprocess.DEVNULL else (None, None)
        if stdin == subprocess.PIPE:
            p.wait(timeout=25)
            out = p.stdout.read()
        print(f"{label} : terminé en {time.monotonic() - t:.2f} s, code {p.returncode}, marqueur {'oui' if b'TSA_AUDIT_OK' in out else 'non'}")
    except subprocess.TimeoutExpired:
        print(f"{label} : TOUJOURS ACTIF après 25 s")
        p.kill()
        p.wait()
