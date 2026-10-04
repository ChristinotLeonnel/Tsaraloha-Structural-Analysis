# IA Co-Engineering de TSA

Assistant d'ingénierie structurelle intégré à TSA : il lit le modèle réel (géométrie, sections,
matériaux, appuis, charges, résultats OpenSees), utilise les outils de TSA et propose des actions
que l'ingénieur valide. Ce n'est pas une autorité : toute recommandation doit être vérifiée par un
ingénieur qualifié.

## Architecture

```text
Interface (src/UI/AI)          AICoEngineeringDock · AIRuntimeDialog · indicateur barre d'état
        │
AIOrchestrator (src/AI/Core)   routage LOCAL/CLOUD/AUTO · confidentialité · contexte · boucle d'outils
        │                      propositions (human-in-the-loop) · diagnostic · auto-benchmark · journal
        ├── EngineeringContextBuilder (src/AI/Context)  données TSA structurées, unités explicites
        ├── AIToolRegistry (src/AI/Tools)               liste blanche d'outils, lecture seule / propositions
        ├── StructuralChecker (src/AI/Checker)          contrôles déterministes (sans LLM)
        ├── EngineeringKnowledgeBase (src/AI/RAG)       BM25 local sur docs/ et documents utilisateur
        └── IAIProvider (src/AI/Providers)
              ├── OpenAICompatibleProvider  protocole Chat Completions (SSE, outils)
              │     ├── llama-server local (LocalLlamaServer, processus séparé)
              │     ├── Ollama (local, optionnel)
              │     └── Cloud compatible OpenAI / Gemini (optionnel, avec accord)
              └── HardwareProfiler + ModelRegistry/ModelSelector + ModelManager (src/AI/Hardware, Models)
```

L'inférence tourne dans un processus `llama-server` séparé, comme OpenSees : l'interface, le
viewport et le solveur ne sont jamais bloqués. Le processus est rattaché à TSA par un Job Object
Windows (`KILL_ON_JOB_CLOSE`) : il ne reste pas en mémoire si TSA se ferme ou plante.

## Matériel et choix du modèle

- **Détection** : CPU (cœurs, AVX2, AVX-512 via CPUID), RAM totale et libre, runtimes (CUDA,
  Vulkan, HIP). Les GPU sont ceux que voit réellement le moteur (`llama-server --list-devices`) :
  aucune hypothèse NVIDIA. Les GPU intégrés (Intel UHD/Iris, Radeon Graphics) sont exclus de
  l'offload recommandé.
- **Registre** : `resources/ai/model_registry.json` (remplaçable par
  `<AppData>/ai/model_registry.json`). Tailles et SHA-256 issus des métadonnées LFS des dépôts
  officiels Qwen ; cache KV calculé depuis `config.json`.
- **Sélection** : mémoire nécessaire = poids + cache KV + tampons. Modes GPU complet, hybride
  CPU+GPU (`--fit on`) ou CPU. Priorité : contexte complet (8192 jetons, nécessaire aux outils),
  puis gamme du modèle, puis quantification.
- **Auto-benchmark** (« Mesurer les performances ») : mesure réelle sur CPU et chaque GPU dédié
  avec le plus petit modèle installé. Les mesures remplacent les estimations. Exemple réel
  (portable i5-11400H + RTX 3050 4 Go, build Vulkan) : CPU 40,8 jetons/s, RTX via Vulkan
  4,4 jetons/s — le GPU détecté n'est donc pas forcément le plus rapide.

## Confidentialité

- Mode par défaut **LOCAL** : aucune donnée ne quitte le poste, Internet non requis.
- **CLOUD / AUTO** : accord demandé avant tout envoi (sauf réglage contraire explicite). Les
  fichiers `.tsa` ne sont jamais envoyés ; seules des données structurées choisies par TSA le sont.
- Clé API chiffrée avec DPAPI (compte Windows). Journal (`<AppData>/ai/logs`) : métadonnées
  uniquement (fournisseur, modèle, latence, jetons, erreurs), jamais le contenu.

## Outils de l'assistant (liste blanche)

Lecture : `get_project_info`, `list_members`, `list_nodes`, `get_object`, `list_load_cases`,
`list_load_combinations`, `list_loads`, `get_results_summary`, `check_model`, `search_knowledge`.

Propositions (rien n'est modifié sans accord) : `propose_section_change` (profils réellement
tabulés : IPE, HEA, HEB, UPN, RECT b×h), `propose_run_analysis` (déclenche la commande de calcul
existante). Une proposition acceptée crée une entrée Undo et notifie les vues.

## Contrôles automatiques (StructuralChecker)

Sans modèle de langage : nœuds isolés ou confondus, barres de longueur nulle ou superposées,
références de nœuds manquantes, structure sans appui, sous-structures non appuyées, translation
d'ensemble non bloquée, appui articulé unique, sections et matériaux invalides, cas de charge vides,
charges orphelines, combinaisons invalides, messages de `LoadValidation`, résultats obsolètes ou
non finis, déplacement maximal supérieur à Lmax/250 (hypothèse indicative, signalée comme telle).
Dalles, voiles et fondations ne sont pas transmis au calcul OpenSees : l'assistant le rappelle.

## Utilisation

- Ruban › Outils › IA Co-Engineering (Assistant IA, Analyser, Vérifier, Expliquer, Configuration)
  et Ruban › Analyse › Co-Engineering. Raccourci du panneau : Ctrl+Maj+I.
- Clic droit sur un élément de l'arbre du modèle › « Expliquer avec l'IA ».
- Barre d'état « IA ● … » : ouvre la configuration (non modale).
- Première utilisation : Configuration › « Installer la configuration recommandée » (téléchargement
  vérifié par SHA-256), puis « Mesurer les performances ».

## Limites actuelles

- What-If (scénarios simulés) et rapport d'ingénierie IA : non implémentés.
- Pas de vérification normative intégrée : l'assistant ne cite une norme que si un document de la
  base de connaissances la contient.
- Les petits modèles (0.6B–1.7B) suivent les consignes de façon approximative ; la qualité des
  explications dépend du modèle installé.
