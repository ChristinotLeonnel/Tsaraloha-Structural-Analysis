---
title: Modèle structural
scope: repo
applies_to: ["src/Model/**"]
---

# 05 — Modèle structural

## Hiérarchie réelle (`src/Model/Element.h`)

```text
Element                     — id(), name(), typeName(), volume(model), weight(model)
├── LinearElement           — startNodeId(), endNodeId(), section(), material(), length(model)
│     ├── Beam              (src/Model/Beam.h)
│     ├── Column            (src/Model/Column.h)
│     ├── TrussMember       (src/Model/TrussMember.h)
│     └── Cable             (src/Model/Cable/Cable.h, + CableAnchor, CableDefinition,
│                             CableGeometry, CablePrestress, CableStandards, CableTypes,
│                             StayCable, SuspensionSystem)
└── SurfaceElement          — nodeIds(), thickness(), material(), area(model)
      ├── Slab              (src/Model/Slab.h)
      └── Wall              (src/Model/Wall.h)
```

`Foundation` (`src/Model/Foundation.h`) existe dans le même dossier :
`TODO: VERIFY IN SOURCE` pour confirmer si elle dérive de `SurfaceElement`, de
`LinearElement`, ou si elle a sa propre interface indépendante — ne pas supposer.

Alias historiques présents dans le code : `ElementLineaire = LinearElement`,
`ElementSurfacique = SurfaceElement`.

## Autres classes centrales

- `Node` (`src/Model/Node.h`) : nœud/point structurel référencé par ID.
- `Section` / `SectionShape` (`src/Model/Section.h`) : voir `.agents/rules/*` section
  dédiée et `docs/SECTIONS.md`.
- `Material`, `MaterialLibrary` (`src/Model/Material.h`,
  `src/Model/MaterialLibrary.h`) : voir `docs/MATERIALS.md`.
- `Model` (`src/Model/Model.h`) : conteneur central, expose `IModelObserver` (interface de
  notification granulaire par type d'élément : `onXAdded/onXModified/onXRemoved`) et
  `ModelStateSnapshot` (utilisé par `UndoRedo::UndoManager` pour undo/redo par restauration
  d'état complet plutôt que par diff incrémental à la commande).
- `ModelDiff` (`src/Model/ModelDiff.h`) : représentation des différences entre deux états du
  modèle — `TODO: VERIFY IN SOURCE` pour son usage exact (comparaison, journalisation, ou
  support d'un futur undo incrémental).
- `StructuralClipboard` (`src/Model/StructuralClipboard.h`) : copier/coller d'éléments
  structuraux — doit préserver le type réel de l'élément (voir règle Sections/Copy-Paste).
- `CreationPresets` (`src/Model/CreationPresets.h`) : valeurs par défaut à la création d'un
  élément dans l'UI.

## Ne pas inventer

Ne jamais introduire une classe (`Truss`, `Frame`, `Panel`, etc.) qui n'existe pas dans
`src/Model` sans vérification explicite au préalable — utiliser le skill
`analyze-project` pour confirmer l'absence avant de proposer une nouvelle classe.

## Pureté du modèle et séparation stricte

- Le modèle structural est la **source de vérité absolue**.
- `src/Model` doit rester strictement découplé de la présentation :
  - ❌ Pas d'inclusion de headers OCCT (`AIS_*`, `V3d_*`, `TopoDS_*`) dans les entités du modèle.
  - ❌ Pas d'inclusion de widgets Qt (`QWidget`, `QDialog`, `QMainWindow`).
  - L'observation du modèle vers l'affichage se fait exclusivement via l'interface abstraite
    `IModelObserver`.
- Les entités structurales (`Beam`, `Column`, `Cable`, `TrussMember`, `Slab`, `Wall`, `Foundation`)
  représentent fidèlement les objets de génie civil et ne doivent jamais être dégradées ou
  fusionnées arbitrairement pour simplifier une implémentation.

## Vérification

`TODO: VERIFY IN SOURCE` pour le contenu détaillé de `Node`, `Foundation`, `ModelDiff`, et
pour toute classe du dossier `src/Model/Cable/` non listée en détail ici.
