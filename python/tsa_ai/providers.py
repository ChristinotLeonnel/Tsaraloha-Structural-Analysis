"""Fournisseurs de modèles de langage : protocole Chat Completions (compatible OpenAI), en ligne ou local.

Un seul client HTTP (bibliothèque standard) pour Gemini (point d'accès compatible OpenAI), OpenRouter, ou un serveur
llama.cpp local. Les erreurs sont typées (réseau, délai, quota, clé, HTTP, réponse mal formée) ; aucun quota n'est
contourné : un HTTP 429 n'est jamais réessayé automatiquement. La clé n'est jamais journalisée.
"""

from __future__ import annotations

import json
import os
import time
import urllib.error
import urllib.request
from dataclasses import dataclass, field
from typing import Any, Optional

# Préréglages vérifiés le 2026-10-19 (pages officielles ; quotas : voir docs/AI_OPERATOR.md). Le modèle reste modifiable.
PRESETS = {
    "gemini": {"base_url": "https://generativelanguage.googleapis.com/v1beta/openai", "model": "", "label": "Google Gemini"},
    "openrouter": {"base_url": "https://openrouter.ai/api/v1", "model": "openrouter/free", "label": "OpenRouter (gratuit)"},
    "local": {"base_url": "http://127.0.0.1:8099/v1", "model": "local", "label": "llama.cpp local"},
    "custom": {"base_url": "", "model": "", "label": "Compatible OpenAI"},   # autre fournisseur configuré dans TSA
}


def _is_local_url(url: str) -> bool:
    return any(h in url for h in ("://127.0.0.1", "://localhost", "://[::1]"))


class ProviderError(RuntimeError):
    """kind : network, timeout, quota, auth, http, malformed, config, cancelled."""

    def __init__(self, kind: str, message: str, retry_after: Optional[float] = None):
        super().__init__(message)
        self.kind = kind
        self.retry_after = retry_after


@dataclass
class ToolCall:
    id: str
    name: str
    arguments: str  # texte JSON tel que reçu (validé ensuite par l'opérateur)


@dataclass
class ChatResult:
    content: str = ""
    tool_calls: list = field(default_factory=list)
    finish_reason: str = ""
    model: str = ""
    latency_ms: int = 0
    prompt_tokens: int = -1
    completion_tokens: int = -1


class AIProvider:
    """Interface commune : un tour de conversation, outils compris."""

    label = "?"
    model = "?"
    remote = True

    def chat(self, messages: list, tools: Optional[list] = None, max_tokens: int = 1536, temperature: float = 0.2) -> ChatResult:
        raise NotImplementedError


class OpenAICompatibleProvider(AIProvider):
    def __init__(self, base_url: str, model: str, api_key: str = "", label: str = "", timeout: float = 180.0, remote: bool = True,
                 extra_body: Optional[dict] = None):
        if not base_url:
            raise ProviderError("config", "URL du fournisseur absente")
        self.base_url = base_url.rstrip("/")
        self.model = model or "default"
        self._api_key = api_key  # jamais exposée (repr, journaux)
        self.label = label or self.base_url
        self.timeout = timeout
        self.remote = remote
        self.extra_body = extra_body or {}

    def __repr__(self) -> str:
        return f"OpenAICompatibleProvider({self.label!r}, model={self.model!r}, key={'oui' if self._api_key else 'non'})"

    @classmethod
    def from_preset(cls, preset: str, model: str = "", api_key: Optional[str] = None, base_url: str = "") -> "OpenAICompatibleProvider":
        if preset not in PRESETS:
            raise ProviderError("config", f"préréglage inconnu « {preset} » ({', '.join(PRESETS)})")
        p = PRESETS[preset]
        key = api_key if api_key is not None else os.environ.get("TSA_AI_API_KEY", "")
        url = base_url or p["base_url"]
        if not url:
            raise ProviderError("config", f"{p['label']} : URL du fournisseur à préciser")
        chosen = model or p["model"]
        if not chosen:
            raise ProviderError("config", f"{p['label']} : nom du modèle à préciser (--model), selon ceux que votre clé autorise")
        remote = not _is_local_url(url)
        if remote and not key:
            raise ProviderError("auth", f"{p['label']} : clé API absente (variable d'environnement TSA_AI_API_KEY)")
        return cls(url, chosen, key, p["label"], remote=remote)

    def _body(self, messages: list, tools: Optional[list], max_tokens: int, temperature: float) -> dict:
        body = {"model": self.model, "messages": messages, "max_tokens": max_tokens, "temperature": temperature, "stream": False}
        if tools:
            body["tools"] = tools
            body["tool_choice"] = "auto"
        body.update(self.extra_body)
        return body

    def chat(self, messages: list, tools: Optional[list] = None, max_tokens: int = 1536, temperature: float = 0.2) -> ChatResult:
        data = json.dumps(self._body(messages, tools, max_tokens, temperature)).encode("utf-8")
        req = urllib.request.Request(self.base_url + "/chat/completions", data=data, method="POST")
        req.add_header("Content-Type", "application/json")
        if self._api_key:
            req.add_header("Authorization", "Bearer " + self._api_key)
        start = time.monotonic()
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                raw = resp.read()
        except urllib.error.HTTPError as e:
            with e:  # libère la connexion de la réponse d'erreur
                detail = e.read()[:400].decode("utf-8", "replace")
            try:
                detail = json.loads(detail).get("error", {}).get("message", detail)
            except (ValueError, AttributeError):
                pass
            if e.code == 429:
                retry = e.headers.get("Retry-After")
                raise ProviderError("quota", f"HTTP 429 — quota ou limite de débit atteint ({detail}). Réessayer plus tard.",
                                    float(retry) if retry and retry.replace(".", "", 1).isdigit() else None) from e
            if e.code in (401, 403):
                raise ProviderError("auth", f"HTTP {e.code} — clé API absente, invalide ou sans droit sur ce modèle ({detail})") from e
            raise ProviderError("http", f"HTTP {e.code} — {detail}") from e
        except TimeoutError as e:
            raise ProviderError("timeout", f"délai de {self.timeout:.0f} s dépassé ({self.label})") from e
        except urllib.error.URLError as e:
            if isinstance(e.reason, TimeoutError):
                raise ProviderError("timeout", f"délai de {self.timeout:.0f} s dépassé ({self.label})") from e
            raise ProviderError("network", f"fournisseur injoignable ({self.label}) : {e.reason}") from e
        return parse_chat_response(raw, int((time.monotonic() - start) * 1000))


def parse_chat_response(raw: bytes, latency_ms: int = 0) -> ChatResult:
    """Réponse Chat Completions → ChatResult ; toute forme inattendue lève ProviderError(« malformed »)."""
    try:
        doc = json.loads(raw)
        choice = doc["choices"][0]
        message = choice["message"]
    except (ValueError, KeyError, IndexError, TypeError) as e:
        raise ProviderError("malformed", f"réponse du fournisseur mal formée : {str(raw[:200])}") from e
    calls = []
    for i, c in enumerate(message.get("tool_calls") or []):
        fn = c.get("function") or {}
        if not isinstance(fn.get("name"), str):
            raise ProviderError("malformed", "appel d'outil sans nom")
        args = fn.get("arguments", "{}")
        calls.append(ToolCall(c.get("id") or f"call_{i}", fn["name"], args if isinstance(args, str) else json.dumps(args)))
    usage = doc.get("usage") or {}
    return ChatResult(content=message.get("content") or "", tool_calls=calls, finish_reason=choice.get("finish_reason") or "",
                      model=doc.get("model", ""), latency_ms=latency_ms, prompt_tokens=usage.get("prompt_tokens", -1),
                      completion_tokens=usage.get("completion_tokens", -1))
