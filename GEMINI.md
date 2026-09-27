# TSA — Contexte pour Gemini

Ce fichier existe pour que Gemini partage le même contexte que les autres agents IA
(Claude, etc.) travaillant sur TSA (Tsaraloha Structural Analysis).

La référence générale des règles est `AGENTS.md` à la racine du dépôt — ne pas la
dupliquer ici. Les règles thématiques détaillées sont dans `.agents/rules/`, les
procédures pas-à-pas dans `.agents/skills/`, les rôles spécialisés dans `.agents/agents/`.

## Documents vivants à consulter avant toute tâche

@docs/ARCHITECTURE.md
@docs/MODEL.md
@docs/UI.md
@docs/OCCT.md
@docs/COORDINATES.md
@docs/SECTIONS.md
@docs/MATERIALS.md
@docs/ROADMAP.md

Ces fichiers documentent l'architecture réelle du dépôt telle que constatée dans le code.
Toute divergence entre ces documents et le code doit être résolue en faveur du code, puis
corrigée dans le document concerné.

Documentation additionnelle déjà présente dans le dépôt (à ne pas remplacer) :
`DOCUMENTATION.md`, `README.md`, `docs/TSALIB_SYSTEM.md`, `docs/TSA_DIAGNOSTICS.md`,
`docs/TSA_FILE_FORMAT.md`, `docs/cable-system/`.

## Méthode obligatoire pour Gemini

```text
ANALYZE → IDENTIFY RESPONSIBILITY → CHECK EXISTING ARCHITECTURE → PLAN → IMPLEMENT → BUILD → TEST → VERIFY
```

- **Recherche préalable systématique :**
  - `SEARCH EXISTING CODE` avant d'écrire du code.
  - `CHECK WHETHER AN EQUIVALENT CLASS ALREADY EXISTS` avant de créer une classe.
  - `CHECK WHETHER AN EXISTING SYSTEM CAN BE EXTENDED` avant de créer un système.
- **Hiérarchie absolue des priorités :**
  ```text
  CORRECTNESS → ARCHITECTURE → MAINTAINABILITY → TESTABILITY → PERFORMANCE → Taille du code
  ```
- **Échelle de surveillance :**
  - `< 300` : confortable | `300-600` : normale | `600-1000` : surveiller | `> 1000` : analyser | `> 2000` : refactoriser | `> 5000` : monolithique.
