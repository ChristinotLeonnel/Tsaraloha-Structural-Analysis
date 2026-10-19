"""Tests unitaires de la couche IA Python (réponses simulées autorisées ici ; les essais réels sont dans test_live.py).

Lancer :  cd python  &&  python -m unittest discover -s tests -v
"""

import http.server
import json
import os
import sys
import tempfile
import threading
import time
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from tsa_ai.audit import AuditLogger  # noqa: E402
from tsa_ai.hardware import (BackendError, RuntimeDevice, classify, runtime_devices, select_backend,  # noqa: E402
                             verify_offload, BackendChoice)
from tsa_ai.providers import ChatResult, OpenAICompatibleProvider, ProviderError, ToolCall, parse_chat_response  # noqa: E402
from tsa_ai.tsa_operator import ActionValidator, TSAOperator  # noqa: E402

RTX = RuntimeDevice("Vulkan1", "vulkan", "NVIDIA GeForce RTX 3050 Laptop GPU", 3964, 3369, "nvidia")
UHD = RuntimeDevice("Vulkan0", "vulkan", "Intel(R) UHD Graphics", 8209, 7421, "intel")


class HardwareTests(unittest.TestCase):
    def test_classification(self):
        self.assertTrue(classify("NVIDIA GeForce RTX 3050 Laptop GPU", 4096).dedicated)
        self.assertFalse(classify("Intel(R) UHD Graphics", 1024).dedicated)
        self.assertFalse(classify("AMD Radeon(TM) Graphics", 512).dedicated)          # APU
        self.assertTrue(classify("AMD Radeon RX 7600", 8192).dedicated)

    def test_auto_selects_dedicated_gpu_and_excludes_intel(self):
        c = select_backend("auto", [UHD, RTX])
        self.assertEqual((c.mode, c.device), ("nvidia", "Vulkan1"))
        self.assertTrue(any("Intel" in n for n in c.notes))

    def test_integrated_only_falls_back_to_cpu(self):
        c = select_backend("auto", [UHD])
        self.assertEqual((c.mode, c.device), ("cpu", "none"))

    def test_no_gpu_falls_back_to_cpu(self):
        self.assertEqual(select_backend("auto", []).device, "none")

    def test_insufficient_memory_falls_back_to_cpu(self):
        self.assertEqual(select_backend("auto", [RTX], min_free_mb=6000).device, "none")

    def test_manual_incompatible_backend_is_explicit_error(self):
        with self.assertRaises(BackendError) as ctx:
            select_backend("amd", [UHD, RTX])
        self.assertIn("CPU", str(ctx.exception))
        with self.assertRaises(BackendError):
            select_backend("nvidia", [UHD])

    def test_manual_cpu(self):
        self.assertEqual(select_backend("cpu", [RTX]).device, "none")

    def test_native_backend_preferred_and_amd_dedicated(self):
        cuda = RuntimeDevice("CUDA0", "cuda", "NVIDIA GeForce RTX 3050 Laptop GPU", 3964, 3300, "nvidia")
        self.assertEqual(select_backend("auto", [RTX, cuda]).device, "CUDA0")
        rocm = RuntimeDevice("ROCm0", "rocm", "AMD Radeon RX 7600", 8176, 7900, "amd")
        self.assertEqual(select_backend("amd", [rocm, RTX]).device, "ROCm0")

    def test_runtime_devices_parsing(self):
        out = "Available devices:\n  Vulkan0: Intel(R) UHD Graphics (8209 MiB, 7421 MiB free)\n" \
              "  Vulkan1: NVIDIA GeForce RTX 3050 Laptop GPU (3964 MiB, 3369 MiB free)\n"
        devs = runtime_devices("llama-server", runner=lambda cmd: out)
        self.assertEqual([d.id for d in devs], ["Vulkan0", "Vulkan1"])
        self.assertEqual(devs[1].free_mb, 3369)
        self.assertEqual(runtime_devices("", runner=lambda cmd: out), [])   # runtime absent

    def test_verify_offload_reads_actual_device(self):
        gpu = BackendChoice("nvidia", "Vulkan1", "RTX")
        log_ok = "llama_model_load_from_file_impl: using device Vulkan1 (NVIDIA GeForce RTX 3050 Laptop GPU)\nload_tensors: offloaded 37/37 layers to GPU\n"
        self.assertTrue(verify_offload(log_ok, gpu)["verified"])
        log_cpu = "load_tensors: offloaded 0/37 layers to GPU\n"
        self.assertEqual(verify_offload(log_cpu, gpu)["actual"], "cpu")            # GPU annoncé mais calcul sur CPU
        log_both = log_ok + "using device Vulkan0 (Intel(R) UHD Graphics)\n"
        self.assertFalse(verify_offload(log_both, gpu)["verified"])                 # le GPU intégré a aussi servi
        self.assertTrue(verify_offload(log_cpu, BackendChoice("cpu", "none", "CPU"))["verified"])


class _Handler(http.server.BaseHTTPRequestHandler):
    behaviour = {}

    def log_message(self, *args):
        pass

    def do_POST(self):
        b = _Handler.behaviour
        self.rfile.read(int(self.headers.get("Content-Length", 0)))
        _Handler.behaviour["auth"] = self.headers.get("Authorization")
        if b.get("sleep"):
            time.sleep(b["sleep"])
        try:
            self.send_response(b.get("status", 200))
            for k, v in b.get("headers", {}).items():
                self.send_header(k, v)
            self.end_headers()
            self.wfile.write(b.get("body", b"{}"))
        except ConnectionError:
            pass  # client parti (essai de délai dépassé) : attendu


class ProviderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), _Handler)
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()
        cls.url = f"http://127.0.0.1:{cls.server.server_address[1]}/v1"

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()

    def provider(self, **kw):
        return OpenAICompatibleProvider(self.url, "m", api_key="secret-key", timeout=kw.get("timeout", 5))

    def test_tool_call_parsed_and_key_sent(self):
        body = {"model": "m", "choices": [{"finish_reason": "tool_calls", "message": {"content": None, "tool_calls": [
            {"id": "c1", "function": {"name": "execute_command", "arguments": "{\"command\":\"query.model_summary\",\"arguments\":{}}"}}]}}]}
        _Handler.behaviour = {"body": json.dumps(body).encode()}
        r = self.provider().chat([{"role": "user", "content": "x"}], tools=[{}])
        self.assertEqual(r.tool_calls[0].name, "execute_command")
        self.assertEqual(_Handler.behaviour["auth"], "Bearer secret-key")
        self.assertNotIn("secret-key", repr(self.provider()))

    def test_quota_429_not_retried(self):
        _Handler.behaviour = {"status": 429, "headers": {"Retry-After": "30"}, "body": b'{"error":{"message":"rate limit"}}'}
        with self.assertRaises(ProviderError) as ctx:
            self.provider().chat([])
        self.assertEqual((ctx.exception.kind, ctx.exception.retry_after), ("quota", 30.0))

    def test_auth_and_http_errors(self):
        _Handler.behaviour = {"status": 401, "body": b'{"error":{"message":"bad key"}}'}
        with self.assertRaises(ProviderError) as ctx:
            self.provider().chat([])
        self.assertEqual(ctx.exception.kind, "auth")
        _Handler.behaviour = {"status": 500, "body": b"oops"}
        with self.assertRaises(ProviderError) as ctx:
            self.provider().chat([])
        self.assertEqual(ctx.exception.kind, "http")

    def test_malformed_response(self):
        _Handler.behaviour = {"body": b"<html>pas du json</html>"}
        with self.assertRaises(ProviderError) as ctx:
            self.provider().chat([])
        self.assertEqual(ctx.exception.kind, "malformed")
        with self.assertRaises(ProviderError):
            parse_chat_response(b'{"choices":[{"message":{"tool_calls":[{"function":{}}]}}]}')

    def test_timeout(self):
        _Handler.behaviour = {"sleep": 2, "body": b"{}"}
        with self.assertRaises(ProviderError) as ctx:
            self.provider(timeout=0.5).chat([])
        self.assertEqual(ctx.exception.kind, "timeout")

    def test_unreachable_provider(self):
        with self.assertRaises(ProviderError) as ctx:
            OpenAICompatibleProvider("http://127.0.0.1:1/v1", "m", timeout=3).chat([])
        self.assertIn(ctx.exception.kind, ("network", "timeout"))

    def test_missing_key_for_remote_preset(self):
        os.environ.pop("TSA_AI_API_KEY", None)
        with self.assertRaises(ProviderError) as ctx:
            OpenAICompatibleProvider.from_preset("openrouter")
        self.assertEqual(ctx.exception.kind, "auth")
        self.assertEqual(OpenAICompatibleProvider.from_preset("local").remote, False)   # modèle local : pas de clé


class FakeClient:
    """Imite le canal de TSA : commandes déclarées, exécution enregistrée (rien n'est exécuté réellement)."""

    def __init__(self, act_enabled=True, verify=True):
        self.act_enabled, self.verify, self.executed = act_enabled, verify, []

    def list_commands(self):
        return [{"id": "model.create_node", "modifies_model": True, "parameters": [{"name": "position", "type": "point3", "unit": "m"}]},
                {"id": "query.model_summary", "modifies_model": False, "parameters": []}]

    def operator_begin(self):
        return {"ok": True, "act_enabled": self.act_enabled}

    def call(self, method, params=None):
        return {"nodes": len(self.executed)}

    def operator_execute(self, command, arguments):
        if not self.act_enabled and command == "model.create_node":
            return {"command": command, "refused": True, "executed": False, "verified": False}
        self.executed.append((command, arguments))
        return {"command": command, "executed": True, "verified": self.verify, "outputs": {"id": len(self.executed)}}

    def read_tool(self, tool, arguments=None):
        return {"tool": tool}


class ScriptedProvider:
    label, model, remote = "simulé", "simulé", False

    def __init__(self, script):
        self.script, self.calls = list(script), 0

    def chat(self, messages, tools=None, **kw):
        self.calls += 1
        item = self.script.pop(0) if self.script else ChatResult(content="fin")
        if isinstance(item, Exception):
            raise item
        return item


def call(name, args):
    return ChatResult(tool_calls=[ToolCall("c", name, args if isinstance(args, str) else json.dumps(args))])


class OperatorTests(unittest.TestCase):
    def audit(self):
        return AuditLogger(os.path.join(tempfile.mkdtemp(), "audit.jsonl"))

    def test_valid_action_executed_and_reported(self):
        client = FakeClient()
        prov = ScriptedProvider([call("execute_command", {"command": "model.create_node", "arguments": {"position": [0, 0, 0]}}),
                                 ChatResult(content="Nœud N1 créé.")])
        rep = TSAOperator(client, prov, self.audit()).run("crée un nœud")
        self.assertEqual(client.executed, [("model.create_node", {"position": [0, 0, 0]})])
        self.assertEqual(rep.text, "Nœud N1 créé.")
        self.assertIn("1 action(s) exécutée(s) et vérifiée(s)", rep.summary())

    def test_malformed_tool_call_is_never_executed(self):
        client = FakeClient()
        prov = ScriptedProvider([call("execute_command", "{pas du json"), call("execute_command", {"command": "os.system", "arguments": {}}),
                                 call("run_python", {"code": "import os"}), ChatResult(content="ok")])
        rep = TSAOperator(client, prov, self.audit()).run("x")
        self.assertEqual(client.executed, [])
        self.assertEqual(len(rep.errors), 3)

    def test_arguments_as_json_text_accepted(self):
        ok, value = ActionValidator({"model.create_node"}).validate("execute_command",
                                                                    json.dumps({"command": "model.create_node", "arguments": "{\"position\":[1,2,3]}"}))
        self.assertTrue(ok)
        self.assertEqual(value["arguments"], {"position": [1, 2, 3]})

    def test_refusal_when_operator_mode_disabled(self):
        client = FakeClient(act_enabled=False)
        prov = ScriptedProvider([call("execute_command", {"command": "model.create_node", "arguments": {"position": [0, 0, 0]}})])
        rep = TSAOperator(client, prov, self.audit()).run("x")
        self.assertTrue(rep.actions[0]["refused"])
        self.assertIn("1 refusée(s)", rep.summary())

    def test_unverified_action_flagged(self):
        prov = ScriptedProvider([call("execute_command", {"command": "model.create_node", "arguments": {"position": [0, 0, 0]}})])
        rep = TSAOperator(FakeClient(verify=False), prov, self.audit()).run("x")
        self.assertEqual(len(rep.unverified), 1)

    def test_round_limit_stops_explicitly(self):
        prov = ScriptedProvider([call("get_project_info", {})] * 10)
        rep = TSAOperator(FakeClient(), prov, self.audit(), max_rounds=3).run("x")
        self.assertIn("NON terminée", rep.stopped)

    def test_provider_unavailable_reported(self):
        prov = ScriptedProvider([ProviderError("network", "injoignable")])
        rep = TSAOperator(FakeClient(), prov, self.audit()).run("x")
        self.assertIn("injoignable", rep.stopped)

    def test_cancel_before_execution(self):
        client = FakeClient()
        prov = ScriptedProvider([call("execute_command", {"command": "model.create_node", "arguments": {"position": [0, 0, 0]}})])
        op = TSAOperator(client, prov, self.audit())
        # run() remet l'annulation à zéro au départ : l'utilisateur annule juste après le début de la demande.
        orig = client.operator_begin
        client.operator_begin = lambda: (op.cancel_event.set(), orig())[1]
        rep = op.run("x")
        self.assertEqual(client.executed, [])
        self.assertIn("annulé", rep.stopped)


class AuditTests(unittest.TestCase):
    def test_secrets_redacted(self):
        log = AuditLogger(os.path.join(tempfile.mkdtemp(), "a.jsonl"))
        e = log.log("x", api_key="sk-1234567890abcdef", headers={"Authorization": "Bearer abc"}, text="clé AIzaSyABCDEFGHIJKLMNOP")
        self.assertEqual(e["api_key"], "<masqué>")
        self.assertEqual(e["headers"]["Authorization"], "<masqué>")
        self.assertEqual(e["text"], "<masqué>")
        self.assertNotIn("sk-1234", open(log.path, encoding="utf-8").read())


if __name__ == "__main__":
    unittest.main()
