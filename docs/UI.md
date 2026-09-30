# Interface Utilisateur — TSA

> Document vivant. Voir aussi `.agents/rules/03-qt-ui.md`. La documentation utilisateur
> détaillée de l'ergonomie existe déjà dans `DOCUMENTATION.md` (sections 3, 4, 8) — ce
> document se concentre sur l'architecture technique de l'UI pour les agents/développeurs.

## Organisation de `src/UI`

```text
src/UI/
├── Ribbon         — ruban de commandes (barre d'outils principale)
├── Dock           — panneaux ancrables (Visibility, Elements, Console, ProjectionView)
├── WindowManager  — gestionnaire centralisé des fenêtres, docks, profils et menu Fenêtres
├── Properties     — PropertyPanel.h/.cpp (panneau de propriétés contextuel)
├── ModelTree      — ModelTreeWidget.h/.cpp (arbre du modèle)
├── Dialogs        — boîtes de dialogue
├── Widgets        — widgets réutilisables
├── Theme          — feuilles de style / thèmes (QSS)
└── Ruler          — règles/graduations du viewport
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

## `WindowManager` & `LayoutManager` (`src/UI/WindowManager`)

Système centralisé de gestion des fenêtres, panneaux et profils de disposition (ISO/IEC/IEEE 26514 / IEEE Std 1063) :

- **`WindowRegistry`** : Registre central unique des fenêtres/docks de l'application avec identifiants stables (`"viewport"`, `"model_browser"`, `"properties"`, `"work_planes"`, `"elements"`, `"visibility"`, `"console"`), état visible/flottant, positions, tailles et raccourcis.
- **`LayoutManager`** : Gestionnaire d'agencement et de profils de disposition.
  - Sauvegarde et restauration binaire native via `QMainWindow::saveState` et `restoreState` (LayoutVersion = 1).
  - Profils prédéfinis : *Modélisation*, *Analyse*, *Résultats*, *Détaillage*, *Personnalisée*.
  - Réinitialisation canonique propre par défaut (`resetLayout()`).
  - Persistance automatique dans `QSettings` au démarrage et à la fermeture (`closeEvent`).
- **`WindowManager`** : Façade applicative et constructeur dynamique du menu `Fenêtres` (`&Fenêtres`).
  - Synchronisation bidirectionnelle automatique (fermeture native via bouton X répercutée instantanément sur la case cochée du menu).
  - Raccourcis centralisés (`Ctrl+1` Vue 3D, `Ctrl+2` Propriétés, `Ctrl+3` Navigateur, `Ctrl+4` Plans de travail, `F2` Console).
  - Extensibilité : tout nouveau panneau s'enregistre via `registerDock()` sans modification manuelle du menu.

## Vérification

`TODO: VERIFY IN SOURCE` pour la liste exacte des signals exposés par `PropertyPanel` et
`ModelTreeWidget`, et pour le détail des widgets présents dans `Ribbon`, `Dock`,
`Dialogs`, `Widgets`, `Theme`, `Ruler` — lire les fichiers correspondants avant
modification structurelle de l'UI.

