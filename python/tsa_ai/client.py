"""Client du canal local de TSA (QLocalServer « tsaraloha-<produit> », protocole JSON une ligne par message).

Windows : tube nommé \\\\.\\pipe\\<nom> ; Linux / macOS : socket Unix <tmp>/<nom>. Aucun port réseau.
Le client ne modifie jamais un fichier de projet : il appelle les méthodes du serveur (docs/MCP.md, docs/AI_OPERATOR.md).
"""

from __future__ import annotations

import json
import os
import socket
import sys
import tempfile
import threading
from typing import Any, Optional

DEFAULT_NAME = "tsaraloha-tsa"


class TSAError(RuntimeError):
    """Erreur rendue par TSA (requête refusée) ou canal indisponible."""


class TSAClient:
    def __init__(self, name: str = DEFAULT_NAME, timeout: float = 120.0):
        self.name = name
        self.timeout = timeout
        self._io = None
        self._sock: Optional[socket.socket] = None
        self._buffer = b""
        self._next_id = 1
        self._lock = threading.Lock()

    # --- Connexion ------------------------------------------------------------------------------------------------
    def connect(self) -> "TSAClient":
        try:
            if sys.platform == "win32":
                # Tube nommé : open() suffit (CreateFile) ; non bufférisé pour ne jamais bloquer sur une lecture partielle.
                self._io = open(r"\\.\pipe\%s" % self.name, "r+b", buffering=0)
            else:
                path = self.name if os.path.isabs(self.name) else os.path.join(tempfile.gettempdir(), self.name)
                self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self._sock.settimeout(self.timeout)
                self._sock.connect(path)
        except OSError as e:
            raise TSAError(f"TSA n'est pas joignable sur le canal « {self.name} » : lancez TSA et ouvrez un projet ({e})") from e
        return self

    def close(self) -> None:
        if self._io:
            self._io.close()
            self._io = None
        if self._sock:
            self._sock.close()
            self._sock = None

    def __enter__(self) -> "TSAClient":
        return self.connect()

    def __exit__(self, *exc) -> None:
        self.close()

    # --- Échange --------------------------------------------------------------------------------------------------
    def _write(self, data: bytes) -> None:
        if self._io:
            self._io.write(data)
        elif self._sock:
            self._sock.sendall(data)
        else:
            raise TSAError("canal TSA non connecté")

    def _read_line(self) -> bytes:
        while b"\n" not in self._buffer:
            chunk = self._io.read(65536) if self._io else self._sock.recv(65536)
            if not chunk:
                raise TSAError("canal TSA fermé par l'application")
            self._buffer += chunk
        line, self._buffer = self._buffer.split(b"\n", 1)
        return line

    def call(self, method: str, params: Optional[dict] = None) -> Any:
        """Appelle une méthode ; rend « result » ou lève TSAError avec le message de TSA."""
        with self._lock:
            rid = self._next_id
            self._next_id += 1
            payload = {"id": rid, "method": method, "params": params or {}}
            self._write((json.dumps(payload, ensure_ascii=False) + "\n").encode("utf-8"))
            while True:
                try:
                    response = json.loads(self._read_line().decode("utf-8"))
                except json.JSONDecodeError as e:
                    raise TSAError(f"réponse illisible du canal TSA : {e}") from e
                if response.get("id") == rid:
                    break
        if "error" in response:
            raise TSAError(response["error"].get("message", "erreur TSA"))
        return response.get("result")

    # --- Raccourcis -----------------------------------------------------------------------------------------------
    def hello(self) -> dict:
        return self.call("hello")

    def list_commands(self) -> list:
        return self.call("list_commands")["commands"]

    def operator_status(self) -> dict:
        return self.call("operator_status")

    def operator_begin(self) -> dict:
        return self.call("operator_begin")

    def operator_execute(self, command: str, arguments: dict) -> dict:
        return self.call("operator_execute", {"command": command, "arguments": arguments})

    def read_tool(self, tool: str, arguments: Optional[dict] = None) -> dict:
        return self.call("read_tool", {"tool": tool, "arguments": arguments or {}})
