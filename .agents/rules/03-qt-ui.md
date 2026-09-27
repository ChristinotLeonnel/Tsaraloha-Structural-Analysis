---
title: Qt & UI
scope: repo
applies_to: ["src/UI/**", "src/App/**"]
---

# 03 — Qt & UI

## Organisation réelle de `src/UI`

```text
src/UI/
├── Ribbon      — ruban de commandes (barre d'outils principale)
├── Dock        — panneaux ancrables
├── Properties  — PropertyPanel (panneau de propriétés contextuel)
├── ModelTree   — ModelTreeWidget (arbre du modèle)
├── Dialogs     — boîtes de dialogue
├── Widgets     — widgets réutilisables
├── Theme       — feuilles de style / thèmes (QSS)
└── Ruler       — règles/graduations du viewport
```

`TODO: VERIFY IN SOURCE` si de nouveaux sous-dossiers apparaissent.

## Règle centrale

```text
UI ≠ source de vérité métier
```

Un widget (`PropertyPanel`, `ModelTreeWidget`, dialogues de création d'éléments) ne stocke
jamais l'état métier de façon autonome : il lit et écrit dans `TSA::Model::Model` (via les
`Commands`/`ICommand` pour les écritures) et se met à jour depuis le modèle (directement ou
via un signal Qt relayant une notification de `IModelObserver`).

## Signals/Slots et Commandes

- Une action utilisateur dans l'UI qui modifie le modèle doit passer par une `ICommand`
  exécutée via `TSA::UndoRedo::CommandManager::executeCommand`, pas par une mutation
  directe du modèle depuis le code UI (sauf lecture seule).
- Voir `src/Commands/CreateBeamCommand.*` comme référence de commande de création
  correctement isolée de l'UI.
- Les signals Qt servent à propager les évènements UI vers les commandes, et les
  notifications `IModelObserver` (ou les signals qui les relaient) à propager les
  changements de modèle vers l'UI. Ne pas créer de canal parallèle qui contournerait l'un
  des deux sens.

## Sélection

- La sélection dans l'UI (ModelTree) et dans le viewport 3D (`SelectionManager`) doivent
  converger vers un état de sélection cohérent référencé au modèle (IDs d'éléments/nœuds),
  pas vers des objets graphiques isolés. Voir `.agents/rules/06-synchronization.md`.

## Synchronisation UI/modèle

- Panneau de propriétés (`PropertyPanel`) : à l'ouverture, se peupler depuis le modèle
  (via l'élément sélectionné) ; à la modification par l'utilisateur, produire une
  `ICommand` plutôt que d'écrire directement dans le modèle.
- Arbre du modèle (`ModelTreeWidget`) : refléter l'état réel du modèle via les callbacks
  `IModelObserver` (ajout/modification/suppression), ne pas maintenir une copie
  indépendante de la liste des éléments.

## Responsabilité des classes UI et découplage

- **Éviter le syndrome de la `MainWindow` monolithique** :
  - `MainWindow` doit orchestrer les composants majeurs (menus, dock widgets, status bar)
    sans implémenter directement : la géométrie OCCT, le solveur de calcul, la sérialisation
    binaire, la logique d'algorithme métier, ou la gestion détaillée des matériaux.
  - Déporter les logiques spécialisées dans des contrôleurs, services ou gestionnaires dédiés
    (ex. `ProjectManager` pour le projet, `CommandManager` pour les actions, `WorkPlaneManager`
    pour les plans de travail).
- **Interdictions de couplage UI** :
  - ❌ Pas de création de `TopoDS_Shape` ou manipulation directe d'`AIS_InteractiveContext`
    dans les fenêtres ou dialogues UI.
  - ❌ Pas de lecture/écriture directe de fichiers disque dans un widget sans passer par la couche
    `IO` ou `ProjectManager`.
  - ❌ Pas d'exécution de solveur FEM directement dans un callback de bouton sans abstraction
    `Analysis`.

## Vérification

`TODO: VERIFY IN SOURCE` pour le détail exact des signals exposés par `PropertyPanel` et
`ModelTreeWidget` — lire les `.h` correspondants avant toute modification.
