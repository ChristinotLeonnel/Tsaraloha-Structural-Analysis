---
title: Git & Tests
scope: repo
applies_to: ["**"]
---

# 07 — Git & Testing

## Git — Gestion Automatique des Branches et Workflow

Pour toute modification du projet TSA, déterminer d'abord la nature du travail avant de modifier le code.
**Principe : NE PAS effectuer automatiquement toutes les modifications dans la branche courante.**

### 1. Conventions de Branches

| Préfixe | Usage | Exemples |
| :--- | :--- | :--- |
| `feature/<nom-court>` | Nouvelle fonctionnalité | `feature/workplane-3d`, `feature/cable-element`, `feature/result-diagrams` |
| `fix/<nom-court>` | Correction de bug existant | `fix/workplane-selection`, `fix/circular-column-display`, `fix/grid-crash` |
| `refactor/<nom-court>` | Refactorisation sans ajout fonctionnel direct | `refactor/coordinate-system`, `refactor/occview-split` |
| `perf/<nom-court>` | Amélioration de performance | `perf/occt-scene-update`, `perf/spatial-indexing` |
| `ui/<nom-court>` | Modification visuelle ou ergonomique | `ui/workplane-marker`, `ui/toolbar-elements` |
| `docs/<nom-court>` | Modification uniquement documentaire / règles | `docs/git-branching-rules`, `docs/workplane-system` |
| `test/<nom-court>` | Ajout ou refonte dédiée aux tests | `test/modular-test-suites` |

### 2. Modifications Communes / Transversales vs Indépendantes

- **Modification commune / transversale liée :**
  Si la modification est très petite, transversale, nécessaire à la tâche actuelle, directement liée à la branche courante, ou constitue une correction mineure indispensable à la fonctionnalité en cours → **rester dans la branche actuelle**.
  *Exemple :* Sur `feature/workplane-3d`, corriger une petite synchronisation du WorkPlane reste sur `feature/workplane-3d`.
- **Modification indépendante :**
  Si une modification n'est pas nécessaire à la tâche actuelle, concerne une autre fonctionnalité ou amélioration indépendante, ou risque de mélanger les livraisons → **NE PAS** l'implémenter dans la branche courante. Créer une branche dédiée (ex. `fix/cable-section-display`).

### 3. Protocole Avant Création d'une Branche

1. Toujours vérifier :
   ```bash
   git status
   git branch --show-current
   ```
2. Identifier la branche actuelle et déterminer si la modification appartient réellement à la tâche courante.
3. Ne jamais créer de branches inutilement.
4. **Branche de base :** Partir de la branche appropriée et à jour (ex. `main`).
   ```bash
   git switch main
   git pull
   git switch -c feature/<nom>
   ```

### 4. Préservation du Travail Existant (Règles Absolues)

- Ne **jamais** supprimer les modifications locales de l'utilisateur.
- Ne **jamais** faire de reset destructif (`git reset --hard` sans demande explicite).
- Ne **jamais** utiliser `git clean -fd` sans autorisation explicite.
- Ne **jamais** écraser des commits existants ou forcer avec `git push --force`.
- Ne **jamais** changer de branche avec des modifications non sauvegardées sans vérifier les conséquences.

### 5. Conventions de Commit

Format des messages de commit :
```text
feat: ...
fix: ...
refactor: ...
perf: ...
ui: ...
docs: ...
test: ...
```

### 6. Push et Fusion

- Pousser la branche de travail correspondante vers le remote après validation (`git push -u origin feature/<nom>`).
- Ne pas pousser automatiquement vers `main`.
- Ne fusionner dans `main` qu'après validation complète (compilation 0 erreur et suite de tests 100% PASS).

### 7. Checklist Obligatoire Avant Toute Modification Importante

Identifier explicitement :
- **TYPE :** `feature` / `fix` / `refactor` / `perf` / `ui` / `docs` / `test`
- **BRANCHE :** branche actuelle ou nouvelle branche
- **JUSTIFICATION :** pourquoi cette modification appartient à cette branche.

## Build & Environnement

- Cible principale : `TSA` (`TSA.exe`).
- Cible de tests : `TSA_Tests` (exécutable `TSA_TestSuite.exe`).
- Cible de benchmarks : `TSA_Benchmark` (exécutable `TSA_Benchmark.exe`).
- Outils de compilation recommandés : CMake avec presets Ninja (`ninja-debug` pour le développement quotidien, `ninja-release` pour la release).
- Utiliser les scripts existants (`scripts/setup_build.ps1`, `CMakePresets.json`) plutôt que des configurations manuelles ad-hoc.

## Stratégie de Test & Validation Proportionnée

Le dépôt dispose d'une suite complète et modulaire de tests automatisés dans `tests/`, intégrée à CTest et pilotée par `TSA_TestSuite.exe`.

### 1. Validation Ciblée au Fil de l'Eau (Développement Quotidien)
Pour éviter de ralentir le développement sur des tâches locales, exécuter uniquement la ou les suites en rapport direct avec la modification :
```powershell
.\build-ninja-debug\TSA_TestSuite.exe --suite=model         # Pour une modification du modèle
.\build-ninja-debug\TSA_TestSuite.exe --suite=coordinates   # Pour une modification des repères/niveaux
.\build-ninja-debug\TSA_TestSuite.exe --suite=commands      # Pour une modification de commande
.\build-ninja-debug\TSA_TestSuite.exe --suite=io            # Pour une modification du format .tsa
.\build-ninja-debug\TSA_TestSuite.exe --suite=opensees      # Pour une modification du calcul FEA
```
*Suites disponibles* : `coordinates`, `model`, `io`, `commands`, `grids`, `viewer`, `cables`, `extensions`, `workplane`, `window`, `node`, `loads`, `opensees`, `supports`, `standards`, `bim`, `ifc`, `drawing`, `views`, `tsa3d`, `modules`, `templates`, `deployment`, `mesh`, `surfaces`, `audit`, `benchmarks`, `display`, `multidoc`, `i18n`, `ribbon`.

### 2. Validation Globale (Avant Push ou Fusion de Branche)
Avant de considérer une branche prête pour la fusion dans la branche de référence :
```powershell
.\build-ninja-debug\TSA_TestSuite.exe                       # Exécution de 100% des suites (100% PASS attendu)
python tools/check_layers.py src                            # Vérification des couches architecturales
python tools/check_shortcuts.py --strict                    # Vérification de la cohérence des raccourcis
```

### 3. Règle d'Intégrité des Tests
- **Règle absolue** : Ne jamais commenter, désactiver ou affaiblir un test existant pour faire passer une modification. Si un test échoue, corriger l'implémentation jusqu'à satisfaction complète du banc d'essais.
- Toute nouvelle fonctionnalité ou entité structurale doit être accompagnée de ses tests unitaires dans `tests/`.

## Validation Finale

Ne déclarer une tâche terminée qu'après compilation réussie (0 erreur) et passage des tests applicables au périmètre de la modification.

