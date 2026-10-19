"""Modèle local facultatif : llama-server (llama.cpp) lancé sur le périphérique choisi par hardware.select_backend.

Jamais lancé automatiquement : seulement sur demande explicite (CLI « --local » ou panneau). --device toujours explicite :
sans lui, llama.cpp répartit les couches sur TOUS les GPU, y compris un GPU intégré. Le périphérique réellement utilisé
est relu dans le journal (hardware.verify_offload) ; s'il diffère du choix, le serveur est arrêté.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request
from pathlib import Path
from typing import Optional

from .hardware import BackendChoice, default_runner, verify_offload


def find_llama_server(explicit: str = "") -> str:
    """llama-server : chemin donné, variable TSA_LLAMA_SERVER, PATH, puis paquet WinGet de llama.cpp."""
    for candidate in (explicit, os.environ.get("TSA_LLAMA_SERVER", ""), shutil.which("llama-server") or ""):
        if candidate and Path(candidate).is_file():
            return candidate
    winget = Path(os.environ.get("LOCALAPPDATA", "")) / "Microsoft" / "WinGet" / "Packages"
    if winget.is_dir():
        for p in winget.glob("ggml.llamacpp*/llama-server.exe"):
            return str(p)
    return ""


class LocalServer:
    def __init__(self, llama_server: str, model_path: str, choice: BackendChoice, port: int = 8099, context: int = 12288,
                 fit_margin_mb: int = 512):
        if not Path(model_path).is_file():
            raise FileNotFoundError(f"modèle local introuvable : {model_path}")
        self.llama_server = llama_server
        self.model_path = model_path
        self.choice = choice
        self.port = port
        self.context = context
        self.fit_margin_mb = fit_margin_mb
        self.process: Optional[subprocess.Popen] = None
        self.log_path = Path(tempfile.gettempdir()) / f"tsa_ai_llama_{port}.log"
        self.verification: dict = {}

    @property
    def base_url(self) -> str:
        return f"http://127.0.0.1:{self.port}/v1"

    def command(self) -> list:
        cmd = [self.llama_server, "-m", self.model_path, "--host", "127.0.0.1", "--port", str(self.port), "--jinja",
               "-c", str(self.context), "--reasoning-budget", "0", "--device", self.choice.device,
               # niveau « debug » : seul niveau où llama.cpp journalise le périphérique et les couches déchargées,
               # indispensables à verify_offload (au niveau 3 par défaut, rien ne prouve l'usage du GPU)
               "-lv", "4"]
        if self.choice.device == "none":
            cmd += ["-ngl", "0"]
        else:
            # Nombre de couches sur le GPU NON imposé : --fit l'ajuste à la mémoire libre du GPU choisi en gardant une
            # marge (un -ngl fixe pourrait saturer une carte de 4 Go ; le reste s'exécute sur le CPU).
            cmd += ["--fit", "on", "--fit-target", str(self.fit_margin_mb), "-fa", "on", "-ctk", "q8_0", "-ctv", "q8_0"]
        return cmd

    def memory_usage(self) -> dict:
        """Mémoire constatée : VRAM utilisée sur les GPU NVIDIA (nvidia-smi) et mémoire de travail du processus."""
        usage: dict = {}
        smi = default_runner(["nvidia-smi", "--query-gpu=name,memory.used,memory.total", "--format=csv,noheader,nounits"])
        usage["gpu"] = [line.strip() for line in smi.splitlines() if line.strip()]
        if self.process and sys.platform == "win32":
            ws = default_runner(["powershell", "-NoProfile", "-Command", f"(Get-Process -Id {self.process.pid}).WorkingSet64"]).strip()
            usage["process_ram_mb"] = int(ws) // (1024 * 1024) if ws.isdigit() else None
        return usage

    def start(self, timeout: float = 180.0) -> dict:
        log = open(self.log_path, "w", encoding="utf-8", errors="replace")
        self.process = subprocess.Popen(self.command(), stdout=log, stderr=subprocess.STDOUT,
                                        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError(f"llama-server arrêté au démarrage (code {self.process.returncode}) ; journal : {self.log_path}")
            try:
                with urllib.request.urlopen(f"http://127.0.0.1:{self.port}/health", timeout=2) as r:
                    if r.status == 200:
                        break
            except OSError:
                time.sleep(1.0)
        else:
            self.stop()
            raise TimeoutError(f"llama-server non prêt après {timeout:.0f} s ; journal : {self.log_path}")
        self.verification = verify_offload(self.log_path.read_text(encoding="utf-8", errors="replace"), self.choice)
        if not self.verification["verified"]:
            self.stop()
            raise RuntimeError(f"périphérique réellement utilisé ({self.verification['actual']}) différent du choix "
                               f"({self.choice.device}) : serveur arrêté")
        return self.verification

    def stop(self) -> None:
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                self.process.kill()
        self.process = None
