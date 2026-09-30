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
├── Port           — PortAreaWidget, PortWidget, PortTypes (espace de travail multi-ports)
├── Diagrams       — Diagram2DWidget (diagrammes interactifs 2D des sollicitations N, V, M, courbes pushover)
├── Dialogs        — boîtes de dialogue
├── Widgets        — widgets réutilisables
├── Theme          — feuilles de style / thèmes (QSS)
└── Ruler          — règles/graduations du viewport
```

## Règle centrale : UI ≠ source de vérité métier

Chaque widget lit et écrit dans `TSA::Model::Model`, jamais l'inverse :

- **Lecture** : le widget se peuple depuis le modèle (directement à l'ouverture, ou en
  continu via les callbacks `IModelObserver`, éventuellement relayés par un signal Qt).
- **Écriture** : toute modification utilisateur produit une `TSA::Commands::ICommand`
  exécutée via `TSA::UndoRedo::CommandManager::executeCommand` — jamais une mutation
  directe du modèle depuis le code UI.

## `RibbonBar` & `RibbonBuilder` (`src/UI/Ribbon`)

Architecture en Ruban structurée en 8 onglets métier :
1. **Accueil** : Gestion du projet/fichiers, historique Undo/Redo, presse-papier, accès rapide (tracé, solveur, résultats 3D) et vue 3D.
2. **Modélisation** : Éléments filaires (poutres, poteaux, câbles, treillis paramétriques), surfaciques (dalles, voiles, semelles), nœuds/primitives et trames/niveaux.
3. **Structure** : Catalogues de sections (I/H, rectangulaire, circulaire), matériaux (béton EC2, acier EC3), conditions d'appuis 3D (encastrements, rotules, appuis simples) et bibliothèques TSALib.
4. **Calcul** : Actions et charges (ponctuelles, réparties, moments, séisme), maillage EF et solveur OpenSees (calcul statique linéaire/non-linéaire, analyse modale, pushover, configuration).
5. **Résultats** : Panneau dock de résultats 3D, déformée amplifiée, 4 familles de diagrammes 3D ($M, V, N, U, R$), réactions vectorielles 3D, cadrage caméra contextuel, note de calcul (NDC) et multiport.
6. **Édition** : Outils de sélection, déplacement 3D, duplication/copie 3D, repère d'origine et suppression.
7. **Affichage** : Projections standards 2D/3D, navigation caméra, plans de travail & coupes 3D, aides visuelles (grilles, niveaux, nœuds) et bascule des panneaux docks.
8. **Outils** : Mesure spatiale 3D, bascule de thème sombre/clair, aide et raccourcis clavier.

## `ResultsDockWidget` (`src/UI/Dock`)

Panneau latéral de pilotage des résultats 3D issu du solveur OpenSees :
- **Grandeur affichée** : Déformée 3D seule ou combinée avec l'une des 4 familles de diagrammes 3D ($M_z, M_y, M_x$, $V_z, V_y$, $N$, $U_x, U_y, U_z, U_{res}$, $R_x, R_y, R_z$).
- **Contrôle d'échelle** : Mode automatique basé sur l'envergure $L_{span}$ de la structure, presets ($\times 1, \times 10, \times 100, \times 1000, \times 10000$) ou facteur personnalisé.
- **Incréments temporels & non-linéaires** : Slider et spinbox pour naviguer entre les pas de calcul avec affichage dynamique du facteur de charge $\lambda$.
- **Filtre des nœuds** : Visibilité conditionnelle (tous, nœuds libres non connectés, appuis).
- **Raccourcis caméra** : Cadrer modèle, cadrer résultats, cadrer déformée, cadrer sélection.

## `AnalysisConfigDialog` (`src/UI/Dialogs`)

Dialogue de configuration avancée de la résolution mécanique :
- Sélection du type d'analyse : Statique linéaire, Statique non-linéaire, Dynamique, Modale.
- Algorithmes non-linéaires : Newton-Raphson standard, LineSearch, ModifiedNewton, Krylov-Newton, BFGS, Broyden, SecantNewton.
- Intégrateurs numériques : LoadControl, DisplacementControl, ArcLength (Crisfield), MinUnbalDispNorm.
- Filtrage contextuel dynamique des paramètres incompatibles en direct.

## `PropertyPanel` (`src/UI/Properties`)

Panneau de propriétés contextuel : affiche/édite les propriétés de l'élément
actuellement sélectionné (section, matériau, dimensions, etc.). Intègre un volet
d'inspection des résultats nodaux OpenSees en lecture seule (déplacements $U$, rotations $R$,
réactions d'appui $F/M$ et diagnostic de connectivité).

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

- **`WindowRegistry`** : Registre central unique des fenêtres/docks de l'application avec identifiants stables (`"viewport"`, `"model_browser"`, `"properties"`, `"work_planes"`, `"elements"`, `"visibility"`, `"console"`, `"results_3d"`), état visible/flottant, positions, tailles et raccourcis.
- **`LayoutManager`** : Gestionnaire d'agencement et de profils de disposition.
  - Sauvegarde et restauration binaire native via `QMainWindow::saveState` et `restoreState` (LayoutVersion = 1).
  - Profils prédéfinis : *Modélisation*, *Analyse*, *Résultats*, *Détaillage*, *Personnalisée*.
  - Réinitialisation canonique propre par défaut (`resetLayout()`).
  - Persistance automatique dans `QSettings` au démarrage et à la fermeture (`closeEvent`).
- **`WindowManager`** : Façade applicative et constructeur dynamique du menu `Fenêtres` (`&Fenêtres`).
  - Synchronisation bidirectionnelle automatique (fermeture native via bouton X répercutée instantanément sur la case cochée du menu).
  - Raccourcis centralisés (`Ctrl+1` Vue 3D, `Ctrl+2` Propriétés, `Ctrl+3` Navigateur, `Ctrl+4` Plans de travail, `Ctrl+5` Résultats 3D, `F2` Console).
  - Extensibilité : tout nouveau panneau s'enregistre via `registerDock()` sans modification manuelle du menu.

