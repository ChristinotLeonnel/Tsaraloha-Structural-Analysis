"""Détection du matériel et choix du périphérique d'inférence du modèle LOCAL (sans rapport avec le calcul EF de TSA).

Règles (choix de l'utilisateur, 2026-10-19) :
  - un modèle local ne s'exécute sur GPU que si le GPU est DÉDIÉ (NVIDIA ou AMD) et réellement vu par le runtime
    (llama-server --list-devices) avec assez de mémoire libre ;
  - jamais sur un GPU intégré (Intel UHD / Iris, Radeon intégrée d'APU…) : pour ne pas surcharger la machine, surtout un
    portable ; sans GPU dédié compatible → CPU ;
  - choix manuel Auto / CPU / NVIDIA / AMD ; un choix incompatible lève une erreur claire qui propose le CPU ;
  - le périphérique annoncé n'est cru qu'après vérification dans le journal du runtime (verify_offload).
Les commandes externes passent par un exécuteur injectable (tests sans matériel).
"""

from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from typing import Callable, Optional

Runner = Callable[[list], str]  # rend la sortie texte (stdout + stderr), "" si la commande échoue


def default_runner(cmd: list) -> str:
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=60, encoding="utf-8", errors="replace",
                             creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        return (out.stdout or "") + (out.stderr or "")
    except (OSError, subprocess.TimeoutExpired):
        return ""


@dataclass
class GPU:
    name: str
    vendor: str            # nvidia, amd, intel, other
    vram_mb: int = 0
    dedicated: bool = False
    reason: str = ""       # pourquoi il est exclu, le cas échéant


@dataclass
class RuntimeDevice:
    id: str                # « Vulkan1 », « CUDA0 », « ROCm0 »
    backend: str           # vulkan, cuda, rocm, …
    name: str
    total_mb: int
    free_mb: int
    vendor: str = "other"


@dataclass
class BackendChoice:
    mode: str              # cpu, nvidia, amd
    device: str            # argument --device de llama-server (« none » pour le CPU)
    label: str
    free_mb: int = 0
    notes: list = field(default_factory=list)


class BackendError(RuntimeError):
    pass


def vendor_of(name: str) -> str:
    n = name.lower()
    if "nvidia" in n or "geforce" in n or "quadro" in n or "rtx" in n or "tesla" in n:
        return "nvidia"
    if "amd" in n or "radeon" in n or "ati " in n:
        return "amd"
    if "intel" in n or "uhd" in n or "iris" in n or n.startswith("arc"):
        return "intel"
    return "other"


_AMD_INTEGRATED = re.compile(r"radeon\(tm\) graphics|radeon graphics|vega \d+ graphics|radeon \d{3}m\b|radeon 610m|radeon 680m|radeon 780m",
                             re.IGNORECASE)


def classify(name: str, vram_mb: int) -> GPU:
    v = vendor_of(name)
    g = GPU(name=name, vendor=v, vram_mb=vram_mb)
    if v == "nvidia":
        g.dedicated = True
    elif v == "amd":
        g.dedicated = not _AMD_INTEGRATED.search(name) and (vram_mb == 0 or vram_mb >= 2048)
        if not g.dedicated:
            g.reason = "GPU AMD intégré (APU) : exclu"
    elif v == "intel":
        g.reason = "GPU Intel (intégré ou non pris en charge) : exclu, CPU à la place"
    else:
        g.reason = "GPU non reconnu : exclu"
    return g


def detect_gpus(runner: Runner = default_runner) -> list:
    """Cartes présentes (sans présumer de leur prise en charge par le runtime)."""
    gpus: list = []
    if sys.platform == "win32":
        out = runner(["powershell", "-NoProfile", "-Command",
                      "Get-CimInstance Win32_VideoController | Select-Object Name,AdapterRAM | ConvertTo-Json -Compress"])
        try:
            items = json.loads(out) if out.strip() else []
            items = items if isinstance(items, list) else [items]
            for it in items:
                ram = int(it.get("AdapterRAM") or 0) // (1024 * 1024)
                gpus.append(classify(str(it.get("Name", "")), ram))
        except (ValueError, TypeError):
            pass
    else:
        out = runner(["lspci"])
        for line in out.splitlines():
            if re.search(r"VGA|3D controller|Display controller", line):
                gpus.append(classify(line.split(":", 2)[-1].strip(), 0))
    # NVIDIA : mémoire exacte (AdapterRAM de WMI est limité à 4 Go)
    smi = runner(["nvidia-smi", "--query-gpu=name,memory.total,memory.free", "--format=csv,noheader,nounits"]) if shutil.which("nvidia-smi") else ""
    for line in smi.splitlines():
        parts = [p.strip() for p in line.split(",")]
        if len(parts) >= 3 and parts[1].isdigit():
            for g in gpus:
                if g.vendor == "nvidia" and (parts[0].lower() in g.name.lower() or g.name.lower() in parts[0].lower()):
                    g.vram_mb = int(parts[1])
    return gpus


_DEVICE_LINE = re.compile(r"^\s*([A-Za-z]+)(\d+):\s*(.+?)\s*\((\d+)\s*MiB,\s*(\d+)\s*MiB free\)", re.MULTILINE)


def runtime_devices(llama_server: str, runner: Runner = default_runner) -> list:
    """Périphériques réellement utilisables par llama.cpp (llama-server --list-devices)."""
    if not llama_server:
        return []
    devices = []
    for m in _DEVICE_LINE.finditer(runner([llama_server, "--list-devices"])):
        backend, index, name, total, free = m.group(1), m.group(2), m.group(3), int(m.group(4)), int(m.group(5))
        devices.append(RuntimeDevice(f"{backend}{index}", backend.lower(), name, total, free, vendor_of(name)))
    return devices


def select_backend(mode: str, devices: list, min_free_mb: int = 0) -> BackendChoice:
    """mode : auto, cpu, nvidia, amd. Seuls les GPU dédiés NVIDIA / AMD vus par le runtime sont éligibles."""
    mode = (mode or "auto").lower()
    if mode not in ("auto", "cpu", "nvidia", "amd"):
        raise BackendError(f"mode inconnu « {mode} » (auto, cpu, nvidia, amd)")
    if mode == "cpu":
        return BackendChoice("cpu", "none", "CPU (choix manuel)")
    notes = []
    eligible = []
    for d in devices:
        g = classify(d.name, d.total_mb)
        if not g.dedicated:
            notes.append(f"{d.id} {d.name} : {g.reason}")
            continue
        if mode != "auto" and g.vendor != mode:
            continue
        if d.free_mb < min_free_mb:
            notes.append(f"{d.id} {d.name} : {d.free_mb} Mio libres < {min_free_mb} Mio nécessaires")
            continue
        eligible.append(d)
    # Backend natif (CUDA, ROCm) préféré à Vulkan pour une même carte, puis la plus grande mémoire libre.
    eligible.sort(key=lambda d: (d.backend not in ("cuda", "rocm", "hip"), -d.free_mb))
    if eligible:
        d = eligible[0]
        return BackendChoice(d.vendor, d.id, f"{d.name} ({d.backend}, {d.free_mb} Mio libres)", d.free_mb, notes)
    if mode in ("nvidia", "amd"):
        raise BackendError(f"Aucun GPU {mode.upper()} dédié compatible avec le runtime ({'; '.join(notes) or 'aucun périphérique'}). "
                           "Choisir le mode CPU.")
    return BackendChoice("cpu", "none", "CPU (aucun GPU dédié NVIDIA/AMD compatible)", 0, notes)


_USING = re.compile(r"using device (\w+)\s*\(([^)]*)\)")
_OFFLOAD = re.compile(r"offloaded (\d+)/(\d+) layers to GPU")


def verify_offload(log_text: str, choice: BackendChoice) -> dict:
    """Constat, dans le journal de llama-server, du périphérique réellement utilisé. Jamais présumé."""
    used = {m.group(1) for m in _USING.finditer(log_text)}
    m = None
    for m in _OFFLOAD.finditer(log_text):
        pass
    offloaded, total = (int(m.group(1)), int(m.group(2))) if m else (0, 0)
    if choice.device == "none":
        ok = offloaded == 0 and not used
        return {"verified": ok, "actual": "cpu" if ok else "gpu inattendu", "devices": sorted(used), "offloaded": offloaded, "layers": total}
    ok = choice.device in used and offloaded > 0 and used <= {choice.device}
    return {"verified": ok, "actual": choice.device if ok else ("cpu" if offloaded == 0 else ",".join(sorted(used))),
            "devices": sorted(used), "offloaded": offloaded, "layers": total}
