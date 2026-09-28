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

## Règle Git — Gestion Automatique des Branches

Ne pas effectuer toutes les modifications dans la branche courante. Identifier avant toute modification importante :
- **TYPE :** `feature` / `fix` / `refactor` / `perf` / `ui` / `docs` / `test`
- **BRANCHE :** branche actuelle ou nouvelle branche (`feature/<nom>`, `fix/<nom>`, `refactor/<nom>`, `ui/<nom>`, `docs/<nom>`, etc.)
- **JUSTIFICATION :** pourquoi cette modification appartient à cette branche.
- **Workflow :**
  - Modification petite et nécessaire à la tâche en cours → conserver sur la branche courante.
  - Modification indépendante → créer une branche dédiée issue de la branche de base à jour.
  - Préservation absolue du travail local (aucun `git reset --hard` ni `git clean -fd` sans accord explicite).
  - Conventions de commit : `feat:`, `fix:`, `refactor:`, `perf:`, `ui:`, `docs:`, `test:`.
  - Push de la branche dédiée (`git push -u origin <branche>`), pas d'écrasement ni de push direct sur `main`.

