# Interface Utilisateur — TSA

> Document vivant. Voir aussi `.agents/rules/03-qt-ui.md`. La documentation utilisateur
> détaillée de l'ergonomie existe déjà dans `DOCUMENTATION.md` (sections 3, 4, 8) — ce
> document se concentre sur l'architecture technique de l'UI pour les agents/développeurs.

## Organisation de `src/UI`

```text
src/UI/
├── Ribbon      — ruban de commandes (barre d'outils principale)
├── Dock        — panneaux ancrables
├── Properties  — PropertyPanel.h/.cpp (panneau de propriétés contextuel)
├── ModelTree   — ModelTreeWidget.h/.cpp (arbre du modèle)
├── Dialogs     — boîtes de dialogue
├── Widgets     — widgets réutilisables
├── Theme       — feuilles de style / thèmes (QSS)
└── Ruler       — règles/graduations du viewport
```

`TODO: VERIFY IN SOURCE` si de nouveaux sous-dossiers apparaissent depuis la rédaction de
ce document.

## Règle centrale : UI ≠ source de vérité métier

Chaque widget lit et écrit dans `TSA::Model::Model`, jamais l'inverse :

- **Lecture** : le widget se peuple depuis le modèle (directement à l'ouverture, ou en
  continu via les callbacks `IModelObserver`, éventuellement relayés par un signal Qt).
- **Écriture** : toute modification utilisateur produit une `TSA::Commands::ICommand`
  exécutée via `TSA::UndoRedo::CommandManager::executeCommand` — jamais une mutation
  directe du modèle depuis le code UI.

## `PropertyPanel` (`src/UI/Properties`)

Panneau de propriétés contextuel : affiche/édite les propriétés de l'élément
actuellement sélectionné (section, matériau, dimensions, etc.). Doit rester cohérent avec
le modèle et avec la géométrie 3D affichée pour ce même élément (voir `.agents/skills/
verify-sync/SKILL.md`).

## `ModelTreeWidget` (`src/UI/ModelTree`)

Arbre reflétant la structure du modèle (éléments, groupes). Doit se maintenir à jour via
les callbacks `IModelObserver` plutôt que via une copie indépendante de la liste des
éléments.

## Sélection

La sélection dans l'arbre (`ModelTreeWidget`) et dans le viewport 3D
(`TSA::Viewer::SelectionManager`) doivent converger vers un même état de sélection
référencé par IDs d'éléments/nœuds du modèle — pas vers des objets graphiques isolés.

## Vérification

`TODO: VERIFY IN SOURCE` pour la liste exacte des signals exposés par `PropertyPanel` et
`ModelTreeWidget`, et pour le détail des widgets présents dans `Ribbon`, `Dock`,
`Dialogs`, `Widgets`, `Theme`, `Ruler` — lire les fichiers correspondants avant
modification structurelle de l'UI.
