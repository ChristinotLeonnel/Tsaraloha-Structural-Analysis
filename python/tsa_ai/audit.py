"""Journal d'audit JSON Lines des demandes, appels d'outils, actions et erreurs — sans secret ni contenu de fichier."""

from __future__ import annotations

import json
import os
import re
import time
from pathlib import Path
from typing import Optional

_SECRET = re.compile(r"(api[_-]?key|authorization|bearer|token|secret|password)", re.IGNORECASE)


def _redact(value):
    if isinstance(value, dict):
        return {k: ("<masqué>" if _SECRET.search(str(k)) else _redact(v)) for k, v in value.items()}
    if isinstance(value, list):
        return [_redact(v) for v in value]
    if isinstance(value, str) and re.search(r"\b(sk-|AIza)[A-Za-z0-9_\-]{10,}", value):
        return "<masqué>"
    return value


class AuditLogger:
    def __init__(self, path: Optional[str] = None):
        default = Path(os.environ.get("LOCALAPPDATA") or Path.home() / ".local" / "share") / "TSA Engineering" / "TSA" / "ai" / "audit.jsonl"
        self.path = Path(path) if path else default
        self.entries: list = []

    def log(self, event: str, **data) -> dict:
        entry = {"t": time.strftime("%Y-%m-%dT%H:%M:%S"), "event": event, **_redact(data)}
        self.entries.append(entry)
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            with open(self.path, "a", encoding="utf-8") as f:
                f.write(json.dumps(entry, ensure_ascii=False) + "\n")
        except OSError:
            pass  # le journal ne doit jamais interrompre une opération
        return entry
