# Modèle structural — TSA

> Document vivant. Source de vérité = code réel dans `src/Model`. Voir aussi
> `.agents/rules/05-structural-model.md`.

## Hiérarchie des éléments (`src/Model/Element.h`)

```text
Element                — id(), name(), typeName(), volume(model), weight(model)
├── LinearElement       — startNodeId(), endNodeId(), section(), material(), length(model)
│     ├── Beam                    (src/Model/Beam.h)
│     ├── Column                  (src/Model/Column.h)
│     ├── TrussMember             (src/Model/TrussMember.h)
│     └── Cable                   (src/Model/Cable/Cable.h)
└── SurfaceElement      — nodeIds(), thickness(), material(), area(model)
      ├── Slab                    (src/Model/Slab.h)
      └── Wall                    (src/Model/Wall.h)
```

Alias historiques présents dans le code : `ElementLineaire = LinearElement`,
`ElementSurfacique = SurfaceElement`.

`Foundation` (`src/Model/Foundation.h`) : `TODO: VERIFY IN SOURCE` — confirmer sa position
exacte dans cette hiérarchie avant de la traiter comme `LinearElement` ou
`SurfaceElement`.

## Système de câbles (`src/Model/Cable/`)

Sous-système riche et spécifique, dédié aux câbles (haubans, suspentes) :
`Cable`, `CableAnalysisProperties`, `CableAnchor`, `CableDefinition`, `CableGeometry`,
`CablePrestress`, `CableStandards`, `CableTypes`, `StayCable`, `SuspensionSystem`. Voir
`docs/cable-system/` (documentation existante détaillée : `overview.md`, `cable-types.md`,
`stay-cables.md`, `suspension-cables.md`, `anchorage.md`, `prestressing.md`,
`geometry.md`, `standards.md`, `analysis.md`) — ne pas dupliquer ce contenu ici.

## `Model` (`src/Model/Model.h`)

Conteneur central de tous les éléments et nœuds. Expose :

- `IModelObserver` — interface de notification granulaire, un triplet
  `onXAdded/onXModified/onXRemoved` par type d'élément (`Node`, `Beam`, `Column`, `Slab`,
  `Wall`, `Foundation`, `TrussMember`, `Cable`, …). C'est le mécanisme réel de
  synchronisation Model → UI/3D (voir `.agents/rules/06-synchronization.md`).
- `ModelStateSnapshot` — état complet utilisé par `TSA::UndoRedo::UndoManager` pour
  undo/redo par restauration d'état plutôt que par diff incrémental par commande.

## Commandes et Undo/Redo

- `TSA::Commands::ICommand` (`src/Commands/ICommand.h`) : interface
  `execute()`/`undo()`/`name()`. Exemple de référence : `CreateBeamCommand`
  (`src/Commands/CreateBeamCommand.h/.cpp`).
- `TSA::UndoRedo::CommandManager` : exécute une `ICommand` et capture l'état undo/redo via
  `UndoManager`.
- `TSA::UndoRedo::UndoManager` : piles de `Model::ModelStateSnapshot` (undo/redo par
  restauration d'état complet, taille max configurable — `maxSteps`, défaut 50).

Toute modification du modèle déclenchée par l'utilisateur doit passer par une `ICommand`
exécutée via `CommandManager::executeCommand`, jamais par une mutation directe du modèle
depuis l'UI.

## Système de Charges & Combinaisons (`src/Model/Load/`)

Gestion complète des actions mécaniques et thermiques selon les Eurocodes :
- **Charges Nodal & Éléments** : `PointLoad` (forces/moments nodaux), `DistributedLoad` (charges linéiques uniformes/trapézoïdales sur barres), `MemberPointLoad`, `ThermalLoad` ($\Delta T$), `SurfaceUniformLoad` (pressions surfaciques sur dalles/voiles).
- **Cas de Charges** : `LoadCase` (type d'action : Permanente $G$, Variable $Q$, Neige $S$, Vent $W$, Séisme $E$, calcul automatique du poids propre `isSelfWeightIncluded`).
- **Combinaisons d'Actions** : `LoadCombination` (combinaisons Eurocodes ELU / ELS avec coefficients partiels $\gamma_G, \gamma_Q, \psi_0$).
- **Gestionnaire Centralisé** : `LoadManager` — gère le cycle de vie, la sérialisation, la suppression en cascade lors de la suppression d'éléments et l'intégration dans `ModelStateSnapshot` pour l'Undo/Redo.

## Système d'Appuis & Liaisons Structuraux (`src/Model/SupportDefinition.h`)

Définition complète des conditions aux limites à 6 degrés de liberté (DDL) :
- **États de DDL (`DOFState`)** : `Free` (Libre), `Fixed` (Bloqué/Encastré rigide), `Spring` (Élastique).
- **6 DDLs indépendants** : 3 translations ($U_x, U_y, U_z$) et 3 rotations ($R_x, R_y, R_z$).
- **Raideurs élastiques** : Constantes de ressort $K_x, K_y, K_z$ (en $\text{kN/m}$) et $K_{rx}, K_{ry}, K_{rz}$ (en $\text{kNm/rad}$).
- **Orientations géométriques (`SupportOrientationType`)** : `Global` (repère général X, Y, Z), `LocalBar` (repère local de la barre connectée), `CustomVector` (vecteur normal $n_x, n_y, n_z$).
- **Usines statiques & Presets** :
  - `SupportDefinition::fixed()` : Encastrement parfait (6 DDLs bloqués).
  - `SupportDefinition::pinned()` : Articulation / Rotule sphérique 3D ($U_x, U_y, U_z$ bloqués).
  - `SupportDefinition::roller()` : Appui simple / Rouleau (translation normale bloquée, glissement libre).
  - `SupportDefinition::sliding()` : Appui glissant / coulisseau.
  - `SupportDefinition::linear()` : Guidage linéaire.
  - `SupportDefinition::planar()` : Appui plan multi-directionnel.
  - `SupportDefinition::elastic()` : Appui sur sol élastique ou ressorts discrets.
  - `SupportDefinition::custom()` : Combinaison personnalisée quelconque.
- **Intégration directe dans `Node`** : Chaque nœud détient son `SupportDefinition`, avec passerelle de compatibilité rétrograde (`supportType()` / `setSupportType()`).

## Autres classes centrales

- `Node` (`src/Model/Node.h`) — nœud/point structurel référencé par ID, porteur des coordonnées et des conditions d'appui `SupportDefinition`.
- `Section` / `SectionShape` — voir `SECTIONS.md`.
- `Material` / `MaterialLibrary` — voir `MATERIALS.md`.
- `ModelDiff` (`src/Model/ModelDiff.h`) — diff d'affichage et journalisation des modifications.
- `StructuralClipboard` (`src/Model/StructuralClipboard.h`) — Copy/Paste d'éléments
  structuraux ; doit préserver le type réel et régénérer la géométrie associée (ne jamais
  partager une `Shape` OCCT entre l'original et la copie).
- `CreationPresets` (`src/Model/CreationPresets.h`) — valeurs par défaut à la création
  d'un élément depuis l'UI.

## Ne pas inventer

Aucune classe (`Truss`, `Frame`, `Panel`, etc.) au-delà de celles listées ci-dessus n'existe
dans `src/Model` à la date de rédaction de ce document. Vérifier via le skill
`analyze-project` avant d'en supposer l'existence ou d'en proposer une nouvelle.
