---
title: Git & Tests
scope: repo
applies_to: ["**"]
---

# 07 — Git & Testing

## Git — Règle Obligatoire : Une Branche Git pour Chaque Intervention

Cette règle est **obligatoire pour toute intervention sur le projet TSA**, qu'elle soit réalisée par un développeur ou un agent IA (**Gemini**, **Antigravity**, **Claude Code** ou tout autre agent).

### 10.1. Créer une branche avant toute modification (Règle B — Vérification préalable)
Avant de modifier, créer ou supprimer un fichier du projet, y compris le code source, les tests, la documentation, les configurations ou les règles :
1. Examiner l'état Git avec `git status`.
2. Identifier la branche courante et les modifications locales non validées.
3. Préserver intégralement les modifications préexistantes, y compris les fichiers non suivis.
4. Créer une nouvelle branche dédiée à l'intervention avant toute modification (`git switch -c <type>/<nom>`).
5. Vérifier que la nouvelle branche est active avant de commencer le travail.

> [!CAUTION]
> **Ne jamais commencer une modification sur `main`, `master` ou une autre branche de travail existante**, sauf autorisation explicite de l'utilisateur.

### 10.2. Convention obligatoire de nommage (Règle A — Une branche dédiée)
Format recommandé : `<type>/<identifiant-court-du-travail>`

| Préfixe | Usage | Exemples |
| :--- | :--- | :--- |
| `feature/` | Nouvelle fonctionnalité | `feature/three-moment-engine`, `feature/ifc-import`, `feature/cable-element` |
| `fix/` | Correction de bug | `fix/beam-torsion`, `fix/circular-column-display`, `fix/grid-crash` |
| `hotfix/` | Correction urgente d'un problème critique | `hotfix/crash-on-open`, `hotfix/license-parser` |
| `refactor/` | Restructuration du code sans changement fonctionnel | `refactor/fem-assembly`, `refactor/coordinate-system`, `refactor/occview-split` |
| `perf/` | Amélioration des performances | `perf/mesh-generation`, `perf/occt-scene-update`, `perf/spatial-indexing` |
| `test/` | Ajout ou modification des tests | `test/opensees-validation`, `test/modular-test-suites` |
| `docs/` | Documentation et références techniques | `docs/git-workflow-rules`, `docs/bim-references` |
| `build/` | Système de compilation et outillage de construction | `build/ninja-msvc-preset`, `build/vcpkg-manifest` |
| `ci/` | Intégration continue et automatisation CI/CD | `ci/ctest-action`, `ci/benchmark-matrix` |
| `research/` | Expérimentation technique isolée sans intégration auto | `research/direct-stiffness-matrix`, `research/non-linear-cables` |

*Choisir un nom court, précis et compréhensible. Ne jamais créer plusieurs branches portant le même nom.*

### 10.3. Une branche par tâche cohérente (Règle F — Travail simultané des agents)
Chaque intervention indépendante doit disposer de sa propre branche :
- Ne pas mélanger une nouvelle fonctionnalité avec des corrections sans rapport.
- Ne pas regrouper plusieurs modifications indépendantes dans une seule branche.
- Pour une tâche complexe, créer une branche principale de travail et, si nécessaire, des branches secondaires clairement reliées.
- Si plusieurs agents travaillent simultanément, attribuer une branche distincte à chaque tâche ou agent.
- Ne jamais laisser plusieurs agents modifier simultanément les mêmes fichiers sans coordination explicite.
- *Une seule branche peut contenir plusieurs modifications si elles sont indispensables à une même tâche cohérente.*

### 10.4. Protection des travaux existants (Règle C — Opérations destructrices interdites)
Avant de créer la branche et à tout moment de l'intervention :
- Ne jamais supprimer les modifications locales de l'utilisateur.
- Ne jamais lancer `git reset --hard`, `git clean -fd`, `git checkout -- .` ou une commande risquant de détruire du travail.
- Ne pas forcer le changement de branche si des modifications locales risquent d'être écrasées.
- Ne pas stash automatiquement les changements sans vérifier leur origine et sans autorisation lorsque cela risque de perturber d'autres agents.
- Ne jamais réécrire l'historique partagé ni forcer (`push --force`).
- *Si l'état du dépôt est ambigu ou si des modifications locales ne peuvent être isolées sans risque, s'arrêter et demander des instructions à l'utilisateur.*

### 10.5. Vérifications après modification (Règle D — Tests et validation)
Sur la branche dédiée :
1. Effectuer uniquement les modifications prévues.
2. Contrôler `git diff` et vérifier qu'aucun fichier étranger n'a été touché.
3. Exécuter les tests pertinents (`TSA_TestSuite`).
4. Vérifier la compilation et les contrôles applicables.
5. Pour les moteurs de calcul de TSA, préserver les validations numériques existantes et effectuer les comparaisons de référence pertinentes (OpenSees, RDM, NAFEMS).
6. Documenter fidèlement les résultats, les échecs et les limitations (ne jamais prétendre un test réussi sans exécution réelle).

### 10.6. Commit, fusion et publication (Règle E — Commit, merge et push)
- **Aucun commit sans autorisation explicite de l'utilisateur** ou règle de projet qui l'autorise clairement.
- **Ne jamais fusionner automatiquement** la branche dans `main` ou dans une autre branche.
- **Ne jamais pousser vers GitHub sans autorisation explicite**.
- Ne jamais supprimer une branche sans vérifier qu'elle ne contient aucun travail utile.
- Fournir un rapport final indiquant : la branche utilisée, les modifications réalisées, les fichiers concernés, les tests exécutés, les résultats, les problèmes restants et les étapes d'intégration.

### 10.7. Cas des tâches de recherche et de lecture seule (Règle B & A)
- Une analyse strictement en lecture seule ne nécessite pas de nouvelle branche tant qu'aucun fichier du projet n'est modifié.
- **Si la recherche implique la création ou le téléchargement de documents destinés au dépôt, créer d'abord une branche `docs/` ou `research/` appropriée.**
- Les téléchargements et résultats temporaires doivent rester hors du dépôt lorsqu'ils ne sont pas destinés à être conservés dans le projet.

### 10.8. Priorité de cette règle & Compatibilité technique (Règle G)
Cette règle complète les règles existantes de TSA. En cas de conflit, privilégier la préservation des travaux existants et l'absence de perte de données. Ne pas supprimer les autres règles et ne pas les remplacer globalement : identifier les éventuelles contradictions et les signaler avant de modifier la politique Git.

> [!IMPORTANT]
> **Compatibilité avec l'évolution de TSA (Règle G)** : Ces règles Git ne doivent pas empêcher l'évolution technique du logiciel (nouveaux moteurs de calcul, correction de bugs, refactorisation, BIM, bibliothèques, optimisation). Elles protègent le travail sans affaiblir les exigences scientifiques, de précision numérique et de sécurité existantes.
> **Principe fondamental : aucune modification non autorisée, aucune perte de travail, aucune intervention non isolée sur une branche existante et aucune fusion ou publication automatique.**



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

