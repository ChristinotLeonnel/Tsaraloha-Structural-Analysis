# Claude Code co-ingénieur de TSA et TSALab (serveur MCP)

Deux façons d'utiliser Claude avec l'écosystème Tsaraloha :

| | Dock « IA Co-Engineering » (dans l'application) | Claude Code + serveur MCP (ce document) |
| :--- | :--- | :--- |
| Qui appelle qui | TSA appelle un modèle (local, ou Cloud dont Anthropic Claude) | Claude Code pilote TSA / TSALab ouverts |
| Compte | clé API (console.anthropic.com), facturée à l'usage | votre compte Claude (abonnement Claude Code) |
| Actions | lecture + propositions validées par l'ingénieur | commandes du registre exécutées en direct (Annuler possible) |

Un abonnement Claude n'est pas une clé API, et une application tierce ne peut pas réutiliser sa connexion :
c'est donc Claude Code, connecté à votre compte, qui se connecte à TSA, et non l'inverse.

## Architecture

```text
Claude Code ──stdio (MCP, JSON-RPC)──► tsaraloha-mcp.exe ──canal local──► AutomationServer (dans TSA / TSALab)
                                        (tools/mcp)          « tsaraloha-tsa »     │  registre de commandes
                                                             « tsaraloha-tsalab »   │  scripts → Blueprint
                                                                                    │  outils de lecture de l'IA
                                                                                    └  Annuler / Rétablir
```

- `src/Automation/AutomationServer.*` (base commune) : `QLocalServer`, accès réservé au compte Windows de
  l'utilisateur (`UserAccessOption`), aucun port réseau. Démarré par `MainWindow` (TSA) et `LabMainWindow` (TSALab).
  Une seule instance par application écoute (la première ouverte).
- `tools/mcp/TsaralohaMcp.cpp` → `tsaraloha-mcp.exe` (Qt Core + Network), construit à côté de `TSA.exe` / `TSALab.exe`.
- Tout passe par l'existant : une commande modifiante = une entrée Annuler, vues mises à jour, console de
  l'application (`MCP › …`) ; les outils de lecture sont ceux de l'assistant IA (liste blanche).

## Outils exposés à Claude

| Outil | Rôle |
| :--- | :--- |
| `tsaraloha_status` | application connectée, version, projet ouvert |
| `list_commands` | commandes du registre : paramètres typés, unités, valeurs par défaut, sorties |
| `execute_command` | une commande : `{"command": "model.create_node", "arguments": {"position": [0, 0, 3]}}` |
| `run_command_script` | script (une commande par ligne, variables `a = …` / `a.id`) exécuté comme un Blueprint |
| `get_model_summary` | contexte d'ingénierie structuré (unités explicites), contrôles en option |
| `inspect_model` | lecture : `get_object`, `list_members`, `list_nodes`, `list_loads`, `get_results_summary`, `check_model`… |
| `undo` / `redo` | Annuler / Rétablir du projet |

Le calcul se lance par la commande `analysis.run` (moteur, axe de grille, export du système) et se lit par
`results.summary`, `results.node_displacement`, `inspect_model get_results_summary`.

## Installation dans Claude Code

1. Compiler TSA ou TSALab (le pont `tsaraloha-mcp.exe` est construit à côté de l'application).
2. Enregistrer le serveur (une fois, portée utilisateur) :

```powershell
claude mcp add --scope user tsalab -- "E:\Book\Dev\TSALab\build-ninja-debug\tsaraloha-mcp.exe" --app TSALab
claude mcp add --scope user tsa    -- "E:\Book\Dev\TSA\build-ninja-debug\tsaraloha-mcp.exe" --app TSA
```

   Sans `--app`, le pont se connecte à TSALab s'il est ouvert, sinon à TSA.
3. Ouvrir TSA ou TSALab avec un projet, puis démarrer (ou redémarrer) une session Claude Code : `/mcp` doit
   lister « tsalab » / « tsa » comme connectés. Demander par exemple : « crée un portique de 6 m sur 3 m avec une
   charge de 12 kN/m et calcule-le ».

## Sécurité

- Canal local nommé, accessible au seul compte Windows qui a lancé l'application ; aucune écoute réseau.
- Pas d'accès aux fichiers ni au système : uniquement les commandes du registre (création, charges, calcul,
  requêtes), les outils de lecture et Annuler / Rétablir. Les fichiers `.tsa` ne sont jamais envoyés.
- Chaque action apparaît dans la console de l'application et s'annule (Ctrl+Z). Les résultats doivent être
  vérifiés par un ingénieur qualifié.

## Tests

Test 200 (suite `automation`) : appels directs du serveur, puis dialogue MCP réel avec `tsaraloha-mcp.exe`
(initialize, tools/list, tools/call, erreur métier signalée, fin propre).
