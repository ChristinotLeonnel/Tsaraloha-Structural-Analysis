"""Opérateur IA : boucle « comprendre → lire → agir → vérifier → conclure » entre un modèle de langage et TSA.

Le modèle ne dispose que d'outils déclarés ici ; chaque appel est validé (ActionValidator) puis exécuté par TSA :
  - execute_command : commande du registre central via l'opérateur C++ du document (politique, Annuler, vérification) ;
  - outils de lecture de l'assistant TSA (read_tool).
Aucun code généré n'est exécuté, aucun fichier n'est lu ni écrit. Le bilan distingue ce que TSA a constaté
(exécuté / vérifié / refusé) de ce que le modèle affirme.
"""

from __future__ import annotations

import json
import threading
from dataclasses import dataclass, field
from typing import Callable, Optional

from .audit import AuditLogger
from .providers import AIProvider, ProviderError

MAX_TOOL_ROUNDS = 40
MAX_TOOL_PAYLOAD = 6000

READ_TOOLS = {
    "get_project_info": ("Informations générales du projet TSA : effectifs, emprise, portée du calcul.", {}),
    "list_nodes": ("Liste paginée des nœuds avec coordonnées (m) et appuis.", {"offset": "integer", "limit": "integer"}),
    "list_members": ("Liste paginée des barres avec nœuds, longueur, section, matériau.", {"type": "string", "offset": "integer", "limit": "integer"}),
    "get_object": ("Détail d'un objet (node, beam, column…) : propriétés, appuis, charges, résultats.", {"type": "string", "id": "integer"}),
    "list_load_cases": ("Cas de charge.", {}),
    "list_loads": ("Charges appliquées, filtrables par cas.", {"load_case_id": "integer", "limit": "integer"}),
    "get_results_summary": ("Synthèse des résultats du dernier calcul.", {}),
    "check_model": ("Contrôles automatiques du modèle (topologie, stabilité, propriétés, charges).", {}),
}

SYSTEM_PROMPT = (
    "Tu es l'opérateur IA de TSA (Tsaraloha Structural Analysis), logiciel de calcul de structures. Réponds en français.\n"
    "Cycle obligatoire : 1) lire l'état réel du projet ; 2) planifier ; 3) exécuter UNE commande à la fois avec "
    "execute_command ; 4) lire son résultat (executed, verified, verification, id créés) ; 5) corriger ou signaler l'échec ; "
    "6) continuer jusqu'à l'objectif ou à un blocage explicite ; 7) conclure par un bilan vérifiable.\n"
    "Règles : n'invente aucun identifiant ; n'annonce jamais un succès si « verified » est faux, si la commande a échoué ou "
    "a été refusée ; n'annonce un calcul terminé que si analysis.run a réussi avec des résultats à jour ; ne modifie jamais "
    "un paramètre d'ingénierie pour faire passer un calcul ; distingue les valeurs CALCULÉES par TSA de tes interprétations ; "
    "n'invente ni norme, ni coefficient, ni capacité ; si une fonction n'existe pas, dis-le. Unités : m, kN, kN/m, degrés."
)


class ActionValidator:
    """Contrôle d'un appel d'outil AVANT tout envoi à TSA."""

    def __init__(self, command_ids: set):
        self.command_ids = command_ids

    def validate(self, name: str, raw_arguments: str) -> tuple:
        """Rend (ok, arguments | message d'erreur)."""
        if name != "execute_command" and name not in READ_TOOLS:
            return False, f"outil non autorisé : {name}"
        try:
            args = json.loads(raw_arguments or "{}")
        except json.JSONDecodeError as e:
            return False, f"arguments JSON invalides ({e.msg}) : corriger l'appel"
        if not isinstance(args, dict):
            return False, "les arguments doivent être un objet JSON"
        if name == "execute_command":
            command = args.get("command")
            inner = args.get("arguments", {})
            if isinstance(inner, str):
                try:
                    inner = json.loads(inner or "{}")
                except json.JSONDecodeError:
                    return False, "« arguments » de execute_command : objet JSON attendu"
            if command not in self.command_ids:
                return False, f"commande inconnue « {command} » : choisir parmi les commandes déclarées"
            if not isinstance(inner, dict):
                return False, "« arguments » de execute_command : objet JSON attendu"
            return True, {"command": command, "arguments": inner}
        return True, args


@dataclass
class OperatorReport:
    text: str = ""
    actions: list = field(default_factory=list)   # résultats bruts de TSA (operator_execute)
    errors: list = field(default_factory=list)
    rounds: int = 0
    stopped: str = ""                             # motif d'arrêt anormal (limite, annulation, fournisseur)

    @property
    def unverified(self) -> list:
        return [a for a in self.actions if a.get("executed") and not a.get("verified")]

    def summary(self) -> str:
        done = sum(1 for a in self.actions if a.get("executed") and a.get("verified"))
        refused = sum(1 for a in self.actions if a.get("refused"))
        failed = sum(1 for a in self.actions if not a.get("executed") and not a.get("refused"))
        return (f"Constaté par TSA : {done} action(s) exécutée(s) et vérifiée(s), {len(self.unverified)} non conforme(s), "
                f"{refused} refusée(s), {failed} en échec.")


def command_signature(spec: dict) -> str:
    params = []
    for p in spec.get("parameters", []):
        t = f"{p['name']}:{p['type']}"
        if p.get("unit"):
            t += f"[{p['unit']}]"
        if not p.get("required", True):
            t += "?"
        params.append(t)
    star = " *" if spec.get("modifies_model") or spec.get("id") == "analysis.run" else ""
    return f"{spec['id']}({', '.join(params)}){star}"


class TSAOperator:
    def __init__(self, client, provider: AIProvider, audit: Optional[AuditLogger] = None,
                 on_event: Optional[Callable[[dict], None]] = None, max_rounds: int = MAX_TOOL_ROUNDS):
        self.client = client
        self.provider = provider
        self.audit = audit or AuditLogger()
        self.on_event = on_event or (lambda e: None)
        self.max_rounds = max_rounds
        self.cancel_event = threading.Event()
        self._commands = client.list_commands()
        self.validator = ActionValidator({c["id"] for c in self._commands})

    def cancel(self) -> None:
        self.cancel_event.set()

    def tool_definitions(self) -> list:
        catalog = "\n".join(command_signature(c) for c in self._commands)
        tools = [{"type": "function", "function": {
            "name": "execute_command",
            "description": ("EXÉCUTE une commande du registre TSA sur le projet ouvert ; le résultat est VÉRIFIÉ par TSA "
                            "(executed, verified, verification). Commandes * : modifient le modèle ou lancent le calcul ; "
                            "refusées si l'utilisateur n'a pas activé le mode opérateur. Point : [x, y, z]. Commandes :\n" + catalog),
            "parameters": {"type": "object", "properties": {
                "command": {"type": "string", "enum": sorted(self.validator.command_ids)},
                "arguments": {"type": "object", "description": "Arguments nommés de la commande"}},
                "required": ["command", "arguments"]}}}]
        for name, (desc, params) in READ_TOOLS.items():
            tools.append({"type": "function", "function": {"name": name, "description": desc, "parameters": {
                "type": "object", "properties": {k: {"type": v} for k, v in params.items()}}}})
        return tools

    def _execute(self, name: str, args: dict) -> dict:
        if name == "execute_command":
            result = self.client.operator_execute(args["command"], args["arguments"])
            self.on_event({"type": "action", "action": result})
            return result
        return self.client.read_tool(name, args)

    def run(self, request: str) -> OperatorReport:
        self.cancel_event.clear()
        report = OperatorReport()
        status = self.client.operator_begin()
        summary = self.client.call("model_summary")
        self.audit.log("request", provider=self.provider.label, model=self.provider.model, act_enabled=status.get("act_enabled"),
                       request=request[:500])
        mode = ("MODE OPÉRATEUR ACTIVÉ par l'utilisateur : tu peux modifier le modèle." if status.get("act_enabled") else
                "MODE OPÉRATEUR DÉSACTIVÉ : seules les lectures sont exécutées ; ne prétends pas avoir modifié le modèle.")
        messages = [{"role": "system", "content": SYSTEM_PROMPT + "\n" + mode},
                    {"role": "system", "content": "État actuel du projet (TSA) : " + json.dumps(summary, ensure_ascii=False)[:4000]},
                    {"role": "user", "content": request}]
        tools = self.tool_definitions()
        while True:
            if self.cancel_event.is_set():
                report.stopped = "annulé par l'utilisateur"
                break
            if report.rounds >= self.max_rounds:
                report.stopped = f"limite de {self.max_rounds} tours d'outils atteinte : tâche NON terminée"
                break
            try:
                result = self.provider.chat(messages, tools)
            except ProviderError as e:
                report.errors.append(f"{e.kind} : {e}")
                report.stopped = f"fournisseur : {e}"
                self.audit.log("provider_error", kind=e.kind, message=str(e))
                break
            self.on_event({"type": "round", "round": report.rounds, "latency_ms": result.latency_ms, "model": result.model})
            if not result.tool_calls:
                report.text = result.content
                break
            report.rounds += 1
            messages.append({"role": "assistant", "content": result.content or None, "tool_calls": [
                {"id": c.id, "type": "function", "function": {"name": c.name, "arguments": c.arguments}} for c in result.tool_calls]})
            for call in result.tool_calls:
                if self.cancel_event.is_set():
                    payload = {"error": "annulé par l'utilisateur : commande non exécutée"}
                else:
                    ok, value = self.validator.validate(call.name, call.arguments)
                    if not ok:
                        payload = {"error": value}
                        report.errors.append(value)
                    else:
                        try:
                            payload = self._execute(call.name, value)
                            if call.name == "execute_command":
                                report.actions.append(payload)
                        except Exception as e:  # erreur rendue par TSA : transmise au modèle, jamais masquée
                            payload = {"error": str(e)}
                            report.errors.append(str(e))
                self.audit.log("tool", name=call.name, arguments=call.arguments[:500], result=json.dumps(payload, ensure_ascii=False)[:500])
                text = json.dumps(payload, ensure_ascii=False)
                messages.append({"role": "tool", "tool_call_id": call.id,
                                 "content": text if len(text) <= MAX_TOOL_PAYLOAD else text[:MAX_TOOL_PAYLOAD] + "…[tronqué]"})
        self.audit.log("finished", rounds=report.rounds, stopped=report.stopped, summary=report.summary())
        return report
