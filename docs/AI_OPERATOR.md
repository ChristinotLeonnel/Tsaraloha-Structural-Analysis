# Opérateur IA de TSA

L'opérateur IA agit réellement dans le projet ouvert : il lit le modèle, crée et modifie les éléments pris en charge,
lance les calculs disponibles, lit les résultats, et **vérifie chaque action dans le modèle**. Il ne simule rien :
une fonction absente du registre de commandes est déclarée absente.

## 1. Architecture

```text
Panneau IA de TSA (Qt)  ──bascule « Python »──►  PythonAIBridge (QProcess, JSON ligne à ligne, asynchrone)
        │                                                    │
        │ assistant intégré (C++)                            ▼
        ▼                                         python -m tsa_ai serve   (python/tsa_ai, bibliothèque standard)
  AIOrchestrator ──► outil execute_command          ├─ fournisseurs : Gemini, OpenRouter, compatible OpenAI, llama.cpp local
        │                                           ├─ matériel : GPU dédié NVIDIA/AMD vérifié, sinon CPU
        ▼                                           └─ TSAOperator : boucle d'action, validation des appels, audit
  AIOperator (C++, un par document)  ◄── canal local tsaraloha-tsa (operator_status / operator_begin / operator_execute)
        │  politique : lecture permise ; modification et calcul seulement en « mode opérateur » ; ≤ 40 actions/demande
        ▼
  CommandRegistry (commandes typées, une entrée Annuler par commande)  ──►  modèle TSA, moteurs de calcul existants
```

- **Une seule couche d'action** : le registre central des commandes (`src/Automation/CommandRegistry`), déjà utilisé par
  l'interface, le Blueprint, la console et le serveur MCP. Python n'écrit jamais dans un fichier de projet.
- **Vérification indépendante** (`src/AI/Operator/AIOperator.cpp`) : après chaque commande, l'état réel est relu (élément
  créé et ses extrémités, rôle poutre / poteau, type d'appui, cas et charges, résultats publiés et à jour). Le résultat
  rendu au modèle distingue `executed` et `verified` ; une vérification non conforme n'est jamais présentée comme un succès.
- **Mode opérateur** : désactivé par défaut. L'utilisateur l'active dans le panneau (bouton « Opérateur »). Un client
  (Python, MCP) ne peut pas l'activer. Pour les essais automatisés seulement : `TSA_AI_OPERATOR=1` au lancement de TSA.
- **Aucune suppression ni écriture de fichier** n'existe dans le registre : l'IA ne peut ni supprimer d'éléments, ni
  remplacer ou écraser un projet. Toute action est annulable (Ctrl+Z).
- **Aucun code généré n'est exécuté** : le modèle ne dispose que d'outils déclarés, dont les appels sont validés
  (`ActionValidator`) avant envoi à TSA.

## 2. Installation

La couche Python n'a **aucune dépendance** : Python 3.9 ou plus suffit. Elle est facultative : TSA fonctionne sans elle,
et l'assistant intégré (C++) reste disponible.

| Système | Installation |
|---|---|
| Windows | installer Python (python.org) ; TSA trouve `python`, `python3` ou `py` dans le PATH, ou `TSA_AI_PYTHON` |
| Linux | paquet `python3` de la distribution |

TSA cherche le paquet dans `TSA_AI_PYTHON_PATH`, puis dans `<dossier de TSA>/python` et ses dossiers parents.

Modèle local (facultatif) : `llama-server` de llama.cpp (`winget install ggml.llamacpp` sous Windows) et un modèle GGUF
(par exemple ceux installés par la configuration IA de TSA). Rien n'est téléchargé automatiquement.

## 3. Configuration d'un fournisseur

La clé n'est **jamais** écrite dans le dépôt ni dans un journal.

- **Dans TSA** : *IA › Configuration › Cloud* : préréglage (Gemini, OpenRouter…), modèle, clé (chiffrée DPAPI sous
  Windows). Le panneau la transmet au service Python par l'environnement du processus uniquement.
- **Autre poste, ligne de commande** : variable d'environnement `TSA_AI_API_KEY`.

```bat
set TSA_AI_API_KEY=<votre clé>
cd TSA\python
python -m tsa_ai chat "Crée un nœud en (0,0,3) m" --preset openrouter
python -m tsa_ai chat "…" --preset gemini --model <modèle autorisé par votre clé>
```

### Conditions vérifiées le 2026-10-19

| Fournisseur | Ce qui a été vérifié | Source |
|---|---|---|
| OpenRouter | catalogue public interrogé : 471 modèles, **16 gratuits (`:free`) acceptant les outils** (ex. `google/gemma-4-31b-it:free`, `openrouter/free`) ; limites des modèles gratuits : 20 requêtes/min, 50/jour (1000/jour après achat d'au moins 10 crédits), HTTP 429 au-delà ; certains fournisseurs peuvent entraîner leurs modèles sur les données (réglage de confidentialité du compte) | [catalogue](https://openrouter.ai/api/v1/models), [limites](https://openrouter.ai/docs/api-reference/limits), [journalisation](https://openrouter.ai/docs/guides/privacy/provider-logging) |
| Gemini API | palier gratuit sans facturation ; quotas par projet, visibles dans AI Studio (la page ne chiffre pas le palier gratuit) ; en palier gratuit, les requêtes peuvent servir à améliorer les produits et être lues par des relecteurs humains : ne pas envoyer d'information confidentielle | [limites](https://ai.google.dev/gemini-api/docs/rate-limits), [conditions](https://ai.google.dev/gemini-api/terms) |

Un abonnement grand public (par exemple Google AI Pro) ne vaut pas accès gratuit illimité à l'API. TSA ne contourne
aucun quota : un HTTP 429 est affiché avec le délai `Retry-After`, sans nouvelle tentative automatique.

**Données envoyées** : la demande, un résumé structuré du modèle (effectifs, cas de charge…) et les résultats des
commandes. Jamais un fichier de projet.

## 4. Matériel du modèle local

Indépendant du calcul aux éléments finis de TSA.

1. Cartes présentes (WMI / `lspci`, `nvidia-smi`) ; 2. périphériques réellement vus par le runtime
(`llama-server --list-devices`) ; 3. choix ; 4. démarrage avec `--device` explicite ; 5. **constat** dans le journal du
runtime (`using device …`, `offloaded n/m layers`) : si le périphérique réel diffère du choix, le serveur est arrêté.

Règles : GPU **dédié** NVIDIA ou AMD seulement, avec assez de mémoire libre ; jamais un GPU intégré (Intel UHD / Iris,
Radeon d'APU) ; sinon CPU. Choix manuel `--device-mode auto|cpu|nvidia|amd` ; un choix incompatible est refusé avec un
message qui propose le CPU. Sans `--device`, llama.cpp répartirait les couches sur tous les GPU, intégré compris.

```bat
python -m tsa_ai hardware                     :: détection seule, aucun modèle lancé
python -m tsa_ai chat "…" --local-model C:\…\Qwen3-4B-Q5_K_M.gguf --device-mode auto
```

Constat sur le poste de développement : RTX 3050 Laptop (Vulkan1, 4 Go) retenue, Intel UHD (Vulkan0) exclue.

## 5. Tests

| Commande | Contenu | Réseau / matériel |
|---|---|---|
| `build-ninja-release\TSA_TestSuite.exe --suite=operator` | tests 481-486, 488, 489 : politique, exécution vérifiée, outil, calcul réel Custom2D, canal Python, pont Python | aucun |
| `TSA_TestSuite.exe --suite=ailive` + `TSA_AI_LIVE_URL`, `TSA_AI_LIVE_MODEL` | test 487 : vrai modèle piloté par l'orchestrateur C++ | fournisseur réel |
| `cd python && python -m unittest discover -s tests` | 26 tests unitaires : matériel, repli CPU, erreurs HTTP (vrai serveur local), 429, réponse mal formée, validation, refus, annulation, audit | aucun |
| idem avec `TSA_AI_INTEGRATION=1`, TSA lancé avec `TSA_AI_OPERATOR=1` sur un projet de test | Python construit une console et la calcule dans le vrai TSA | TSA |
| idem avec `TSA_AI_LIVE_PRESET=gemini\|openrouter\|local` (+ `TSA_AI_API_KEY`) | requête minimale, appel d'outil structuré, scénario réel | fournisseur réel |

Les essais réels sont « skipped » quand leur configuration manque : ils ne sont jamais comptés comme réussis.

Remarque : `TSA_TestSuite.exe` lancé avec `QT_QPA_PLATFORM=offscreen` s'arrête au démarrage (0xC0000409) sur ce poste,
avant tout test ; sans cette variable, les suites s'exécutent.

## 6. Dépannage

| Symptôme | Cause / remède |
|---|---|
| « TSA n'est pas joignable sur le canal » | lancer TSA et ouvrir un projet ; le canal sert le document au premier plan |
| action « refusée » | mode opérateur désactivé : bouton « Opérateur » du panneau |
| HTTP 401 / 403 | clé absente, invalide ou sans droit sur le modèle |
| HTTP 429 | quota du fournisseur atteint : attendre le délai indiqué |
| « nom du modèle à préciser » (Gemini) | choisir un modèle autorisé par la clé (`--model`) |
| matériel : CPU alors qu'un GPU existe | GPU intégré, mémoire libre insuffisante, ou runtime sans ce périphérique (`python -m tsa_ai hardware`) |

## 7. Limites connues

- Commandes disponibles : nœuds, poutres, poteaux, appuis, sections, cas et charges (nodales, réparties), calcul,
  résultats, comparaison. Pas de dalle, de suppression, ni de commande d'affichage (par exemple « montrer la déformée ») :
  l'IA le dit. Le panneau Résultats de TSA s'actualise après un calcul.
- `analysis.run` est synchrone dans TSA : l'interface attend la fin du calcul.
- Pas d'automatisation de l'interface par accessibilité (UIA) depuis l'opérateur : les actions passent par les commandes.
- Canal sous Linux / macOS (socket Unix) : écrit, non essayé.
