"""Ligne de commande : python -m tsa_ai {hardware|status|chat|serve} (voir docs/AI_OPERATOR.md)."""

from __future__ import annotations

import argparse
import json
import sys

from .client import DEFAULT_NAME, TSAClient, TSAError
from .hardware import BackendError, runtime_devices, select_backend
from .local import LocalServer, find_llama_server
from .providers import PRESETS, OpenAICompatibleProvider, ProviderError
from .service import hardware_report, serve
from .tsa_operator import TSAOperator


def main(argv=None) -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser(prog="tsa_ai", description="Opérateur IA de TSA (couche Python).")
    ap.add_argument("--pipe", default=DEFAULT_NAME, help="canal local de TSA (défaut : tsaraloha-tsa)")
    sub = ap.add_subparsers(dest="cmd", required=True)

    hw = sub.add_parser("hardware", help="matériel détecté et périphérique d'inférence retenu (ne lance aucun modèle)")
    hw.add_argument("--mode", default="auto", choices=["auto", "cpu", "nvidia", "amd"])
    hw.add_argument("--llama-server", default="")

    sub.add_parser("status", help="connexion à TSA et état du mode opérateur")

    ch = sub.add_parser("chat", help="exécute une demande en langage naturel dans le projet ouvert")
    ch.add_argument("request")
    ch.add_argument("--preset", default="gemini", choices=sorted(PRESETS))
    ch.add_argument("--model", default="")
    ch.add_argument("--base-url", default="")
    ch.add_argument("--local-model", default="", help="GGUF à lancer avec llama-server (lancement explicite)")
    ch.add_argument("--device-mode", default="auto", choices=["auto", "cpu", "nvidia", "amd"])
    ch.add_argument("--min-free-mb", type=int, default=1500,
                    help="mémoire GPU libre minimale pour accepter le GPU (--fit place sur le GPU ce qui y tient, le reste sur le CPU)")
    ch.add_argument("--context", type=int, default=12288, help="contexte du modèle local (jetons)")

    sub.add_parser("serve", help="service JSON (stdin/stdout) du panneau IA de TSA")
    args = ap.parse_args(argv)

    if args.cmd == "hardware":
        print(json.dumps(hardware_report(args.mode, args.llama_server), ensure_ascii=False, indent=2))
        return 0
    if args.cmd == "serve":
        return serve(args.pipe)
    try:
        with TSAClient(args.pipe) as client:
            if args.cmd == "status":
                print(json.dumps({"hello": client.hello(), "operator": client.operator_status()}, ensure_ascii=False, indent=2))
                return 0
            local = None
            try:
                if args.local_model:
                    server = find_llama_server()
                    choice = select_backend(args.device_mode, runtime_devices(server), args.min_free_mb)
                    print(f"[matériel] {choice.label} ; démarrage de llama-server…", file=sys.stderr)
                    local = LocalServer(server, args.local_model, choice, context=args.context)
                    print(f"[matériel] constaté : {json.dumps(local.start(), ensure_ascii=False)}", file=sys.stderr)
                    provider = OpenAICompatibleProvider(local.base_url, "local", label=f"llama.cpp local ({choice.label})", remote=False)
                else:
                    provider = OpenAICompatibleProvider.from_preset(args.preset, args.model, base_url=args.base_url)
                op = TSAOperator(client, provider, on_event=lambda e: print("[action] " + json.dumps(e["action"], ensure_ascii=False)[:300],
                                                                           file=sys.stderr) if e.get("type") == "action" else None)
                report = op.run(args.request)
            finally:
                if local:
                    local.stop()
            print(report.text)
            print("\n" + report.summary())
            if report.stopped:
                print("Arrêt : " + report.stopped)
            return 0 if not report.stopped and not report.unverified else 2
    except (TSAError, ProviderError, BackendError, FileNotFoundError, RuntimeError, TimeoutError) as e:
        print(f"Erreur : {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
