"""Service du panneau IA de TSA : une requête JSON par ligne sur stdin, un événement JSON par ligne sur stdout.

Requêtes : {"type": "configure", "preset", "model", "base_url"} · {"type": "chat", "text"} · {"type": "cancel"} ·
           {"type": "hardware", "mode", "llama_server"} · {"type": "quit"}.
Événements : ready, configured, status, action, round, final, error, hardware.
La demande s'exécute dans un fil séparé (annulable) ; la clé API vient de l'environnement du processus (TSA_AI_API_KEY),
jamais d'un message.
"""

from __future__ import annotations

import json
import sys
import threading
from dataclasses import asdict

from . import __version__
from .client import TSAClient, TSAError
from .hardware import BackendError, detect_gpus, runtime_devices, select_backend
from .local import find_llama_server
from .providers import OpenAICompatibleProvider, ProviderError
from .tsa_operator import TSAOperator

_out_lock = threading.Lock()


def emit(event: dict) -> None:
    with _out_lock:
        sys.stdout.write(json.dumps(event, ensure_ascii=False) + "\n")
        sys.stdout.flush()


def hardware_report(mode: str = "auto", llama_server: str = "") -> dict:
    server = find_llama_server(llama_server)
    devices = runtime_devices(server)
    report = {"gpus": [asdict(g) for g in detect_gpus()], "llama_server": server or None,
              "runtime_devices": [asdict(d) for d in devices]}
    try:
        report["choice"] = asdict(select_backend(mode, devices))
    except BackendError as e:
        report["choice"] = None
        report["error"] = str(e)
    return report


def serve(pipe_name: str) -> int:
    provider = None
    current: dict = {}
    emit({"type": "ready", "version": __version__})
    for line in sys.stdin:
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            emit({"type": "error", "kind": "malformed", "message": "requête illisible"})
            continue
        kind = msg.get("type")
        if kind == "quit":
            break
        if kind == "configure":
            try:
                provider = OpenAICompatibleProvider.from_preset(msg.get("preset", "gemini"), msg.get("model", ""),
                                                                base_url=msg.get("base_url", ""))
                emit({"type": "configured", "provider": provider.label, "model": provider.model, "remote": provider.remote})
            except ProviderError as e:
                provider = None
                emit({"type": "error", "kind": e.kind, "message": str(e)})
        elif kind == "hardware":
            emit({"type": "hardware", **hardware_report(msg.get("mode", "auto"), msg.get("llama_server", ""))})
        elif kind == "cancel":
            if current.get("operator"):
                current["operator"].cancel()
        elif kind == "chat":
            if provider is None:
                emit({"type": "error", "kind": "config", "message": "aucun fournisseur configuré"})
                continue
            if current.get("thread") and current["thread"].is_alive():
                emit({"type": "error", "kind": "busy", "message": "une demande est déjà en cours"})
                continue

            def work(text: str, p=provider) -> None:
                try:
                    with TSAClient(pipe_name) as client:
                        op = TSAOperator(client, p, on_event=emit)
                        current["operator"] = op
                        report = op.run(text)
                        emit({"type": "final", "text": report.text, "summary": report.summary(), "stopped": report.stopped,
                              "errors": report.errors, "actions": report.actions, "provider": p.label, "model": p.model})
                except TSAError as e:
                    emit({"type": "error", "kind": "tsa", "message": str(e)})
                finally:
                    current["operator"] = None

            current["thread"] = threading.Thread(target=work, args=(msg.get("text", ""),), daemon=True)
            current["thread"].start()
        else:
            emit({"type": "error", "kind": "malformed", "message": f"type de requête inconnu : {kind}"})
    return 0
