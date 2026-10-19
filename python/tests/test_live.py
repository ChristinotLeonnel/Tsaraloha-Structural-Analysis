"""Essais RÉELS, séparés des tests unitaires ; jamais déclarés réussis sans exécution réelle (sinon : « skipped »).

1) IntegrationTests (TSA_AI_INTEGRATION=1) : TSA lancé avec un projet de test et TSA_AI_OPERATOR=1. Le fournisseur est
   scripté (décisions simulées, adaptées aux identifiants rendus), mais CHAQUE commande passe réellement par le canal,
   l'opérateur C++ (Annuler, vérification) et le moteur de calcul de TSA.
2) LiveProviderTests (TSA_AI_LIVE_PRESET = gemini | openrouter | local, + TSA_AI_API_KEY pour un fournisseur en ligne,
   TSA_AI_LIVE_MODEL facultatif) : vraie requête réseau, puis vrai scénario dans TSA si TSA_AI_INTEGRATION=1.
"""

import json
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from tsa_ai.client import TSAClient  # noqa: E402
from tsa_ai.providers import ChatResult, OpenAICompatibleProvider, ToolCall  # noqa: E402
from tsa_ai.tsa_operator import TSAOperator  # noqa: E402

INTEGRATION = os.environ.get("TSA_AI_INTEGRATION") == "1"
LIVE_PRESET = os.environ.get("TSA_AI_LIVE_PRESET", "")
PIPE = os.environ.get("TSA_AI_PIPE", "tsaraloha-tsa")

REQUEST = ("Construis une console : nœud en (0,0,0), nœud en (4,0,0), poutre IPE 300 entre eux, encastrement (fixed) en (0,0,0), "
           "cas de charge P (live), charge nodale fz = -10 kN en (4,0,0) dans ce cas, puis calcule avec engine=custom2d, "
           "axis=plan, self_weight=non, case=P, et donne la flèche maximale calculée par TSA.")


class AdaptiveConsoleProvider:
    """Décisions simulées d'un modèle : enchaîne les commandes en réutilisant les « id » réellement rendus par TSA."""

    label, model, remote = "scénario scripté (intégration)", "scripté", False

    def __init__(self):
        self.step = 0
        self.ids = []

    def chat(self, messages, tools=None, **kw):
        last = messages[-1]
        if last.get("role") == "tool":
            out = json.loads(last["content"]).get("outputs", {})
            if "id" in out:
                self.ids.append(int(out["id"]))
        plan = [
            lambda: ("model.create_node", {"position": [0, 0, 0]}),
            lambda: ("model.create_node", {"position": [4, 0, 0]}),
            lambda: ("model.create_beam", {"start": self.ids[0], "end": self.ids[1], "section": "IPE 300"}),
            lambda: ("model.set_support", {"node": self.ids[0], "type": "fixed"}),
            lambda: ("loads.create_case", {"name": "P", "category": "live"}),
            lambda: ("loads.add_nodal", {"node": self.ids[1], "case": self.ids[3], "fz": -10}),
            lambda: ("analysis.run", {"engine": "custom2d", "axis": "plan", "self_weight": "non", "case": "P"}),
            lambda: ("results.summary", {}),
        ]
        if self.step < len(plan):
            command, args = plan[self.step]()
            self.step += 1
            return ChatResult(tool_calls=[ToolCall(f"c{self.step}", "execute_command", json.dumps({"command": command, "arguments": args}))])
        return ChatResult(content="Scénario terminé (bilan : voir les actions constatées par TSA).")


@unittest.skipUnless(INTEGRATION, "TSA_AI_INTEGRATION=1 non défini : TSA non lancé pour cet essai")
class IntegrationTests(unittest.TestCase):
    def test_python_operator_builds_and_computes_in_real_tsa(self):
        with TSAClient(PIPE) as client:
            hello = client.hello()
            self.assertTrue(client.operator_status()["act_enabled"], "lancer TSA avec TSA_AI_OPERATOR=1 pour cet essai")
            before = client.call("model_summary")
            report = TSAOperator(client, AdaptiveConsoleProvider()).run(REQUEST)
            print("\n  TSA :", hello.get("product"), hello.get("version"), "—", report.summary())
            for a in report.actions:
                print("  ", a.get("command"), "→ exécutée" if a.get("executed") else "→ NON exécutée",
                      "/ vérifiée" if a.get("verified") else "/ NON vérifiée", "—", a.get("message", "")[:120])
            self.assertEqual(report.stopped, "")
            self.assertEqual(len(report.actions), 8)
            self.assertTrue(all(a["executed"] and a["verified"] for a in report.actions), report.summary())
            run = next(a for a in report.actions if a["command"] == "analysis.run")
            self.assertTrue(run["verification"]["results_up_to_date"])
            # Valeur CALCULÉE par TSA, comparée à P·L³/3EI (IPE 300 : Iy relu dans TSA)
            bar = client.operator_execute("query.bar", {"id": report.actions[2]["outputs"]["id"]})["outputs"]
            expected = 10.0 * 4.0 ** 3 / (3.0 * bar["e"] * bar["iy"])
            self.assertAlmostEqual(run["outputs"]["max_displacement"], expected, delta=1e-3 * expected)
            after = client.call("model_summary")
            print("  modèle avant / après :", json.dumps(before)[:160], "→", json.dumps(after)[:160])


@unittest.skipUnless(LIVE_PRESET, "TSA_AI_LIVE_PRESET non défini : aucun fournisseur réel configuré")
class LiveProviderTests(unittest.TestCase):
    def provider(self):
        return OpenAICompatibleProvider.from_preset(LIVE_PRESET, os.environ.get("TSA_AI_LIVE_MODEL", ""),
                                                    base_url=os.environ.get("TSA_AI_LIVE_URL", ""))

    def test_minimal_request(self):
        p = self.provider()
        r = p.chat([{"role": "user", "content": "Réponds seulement : OK"}], max_tokens=16)
        print(f"\n  {p.label} / {r.model or p.model} : {r.content!r} en {r.latency_ms} ms")
        self.assertTrue(r.content.strip())

    def test_structured_tool_call(self):
        p = self.provider()
        tools = [{"type": "function", "function": {"name": "execute_command", "description": "Exécute une commande TSA",
                  "parameters": {"type": "object", "properties": {"command": {"type": "string", "enum": ["model.create_node"]},
                                                                  "arguments": {"type": "object"}}, "required": ["command", "arguments"]}}}]
        r = p.chat([{"role": "user", "content": "Crée un nœud en (0, 0, 3) m avec l'outil."}], tools=tools, max_tokens=256)
        print(f"\n  appel d'outil : {[(c.name, c.arguments) for c in r.tool_calls]} en {r.latency_ms} ms")
        self.assertTrue(r.tool_calls, "le modèle n'a pas produit d'appel d'outil")
        args = json.loads(r.tool_calls[0].arguments)
        self.assertEqual(args["command"], "model.create_node")

    @unittest.skipUnless(INTEGRATION, "TSA_AI_INTEGRATION=1 non défini")
    def test_real_model_operates_tsa(self):
        with TSAClient(PIPE) as client:
            report = TSAOperator(client, self.provider()).run(REQUEST)
            print("\n  " + report.summary() + "\n  " + report.text[:800])
            self.assertFalse(report.unverified)
            self.assertTrue(any(a["command"] == "analysis.run" and a["verified"] for a in report.actions))


if __name__ == "__main__":
    unittest.main()
