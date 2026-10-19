# Note de continuité — opérateur IA de TSA (C++ outils, Python cerveau)

Note de reprise courte (non publiée). Documentation complète : `docs/AI_OPERATOR.md`.

## État Git (2026-10-19)

- Branche `feature/ai-operator` (depuis `main` = `21cbb2e`). **Aucun commit, aucun push.**
- Modifications préexistantes d'une autre session, **préservées** : `cmake/TSAProduct.cmake` (MockAIProvider,
  AIProviderRouter), `src/AI/Core/AIOrchestrator.cpp` (préréglages Cloud), `src/UI/AI/AIRuntimeDialog.cpp`,
  `tests/test_ai.cpp` (« tests 124-127 »), non suivis `docs/CO_ENGINEERING/`, `empty_project.tsa`,
  `scripts/autonomous_ui_test.py`, `src/AI/Providers/{AIProviderRouter,MockAIProvider}.*`, `tests/results/`.
  Mes ajouts dans ces fichiers sont additifs (voir `git diff`).

## Décisions de l'utilisateur

- Architecture « Python cerveau, C++ outils » (Python : boucle IA, fournisseurs, matériel ; C++ : commandes, opérateur,
  vérification, panneau). L'assistant C++ reste disponible.
- Modèle local : GPU dédié NVIDIA/AMD seulement, jamais un GPU intégré (Intel UHD) ; sinon CPU. Demander avant de lancer.

## Fait et vérifié

| Élément | Preuve |
|---|---|
| `src/AI/Operator/AIOperator` : exécution par le registre, politique, vérification du modèle | `TSA_TestSuite --suite=operator` : 8/8 |
| outil `execute_command`, orchestrateur (cycle d'action, 40 tours, arrêt explicite), panneau (bascules Opérateur et Python, historique) | suite `ai` : 24/24 (test 118 conservé) |
| canal `operator_status / operator_begin / operator_execute` (AutomationServer) | test 488 ; suite `automation` 3/3 |
| `python/tsa_ai` (stdlib) : client du canal, fournisseurs, matériel, serveur local, opérateur, audit, service, CLI | `python -m unittest discover -s tests` : 26 OK, 4 skipped |
| Python dans le **vrai TSA** (projet de test, `TSA_AI_OPERATOR=1`) : console + calcul Custom2D | 8/8 actions exécutées et vérifiées ; δmax 12,157 mm = PL³/3EI ; M = 40 kN·m ; capture d'écran |
| pont `PythonAIBridge` (vrai processus Python) | test 489 |
| détection réelle : RTX 3050 (Vulkan1) retenue, Intel UHD exclue | `python -m tsa_ai hardware` (aucun modèle lancé) |

## Bloqué (attend l'utilisateur)

1. **Fournisseur en ligne** : aucune clé sur le poste. Avec une clé : `set TSA_AI_API_KEY=…`,
   `set TSA_AI_LIVE_PRESET=openrouter` (ou `gemini` + `TSA_AI_LIVE_MODEL`), puis
   `cd python && python -m unittest tests.test_live -v` (avec TSA lancé et `TSA_AI_INTEGRATION=1` pour le scénario).
2. **Modèle local** : non lancé (accord requis). Contrôles faits : Qwen3 4B Instruct Q5_K_M présent (2,69 Go,
   36 couches, 8 têtes KV) ; VRAM libre 3 964 Mio ; `-ngl 99` remplacé par `--fit on --fit-target 512`, contexte 12 288.
   Essai complet prêt : TSA lancé avec `TSA_AI_OPERATOR=1` sur une copie de projet, puis
   `python tests\run_live_local.py <gguf> <rapport.json>` (S1 lecture, S2 console + calcul, S3 échec, S4 interdit,
   S5 interruption). Commande CLI simple :
   `python -m tsa_ai chat "…" --local-model "%LOCALAPPDATA%\TSA Engineering\TSA\ai\models\Qwen3-4B-Q5_K_M.gguf"`
   (choisit Vulkan1, vérifie le périphérique réel dans le journal).

## Reste à faire

- Relecture du diff ; commit sur `feature/ai-operator` avec l'accord de l'utilisateur.
- Commande d'affichage (déformée) et outils UI : absents du registre, à concevoir.
- Installateur : copier `python/tsa_ai` à côté de `TSA.exe`.

## Commandes

```bat
cmd /c ""C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cmake --build --preset ninja-release --target TSA_Tests TSA -- -k 0"
build-ninja-release\TSA_TestSuite.exe --suite=operator
cd python && python -m unittest discover -s tests -v
python -m tsa_ai hardware
```
