---
name: add-element
description: Ajouter un nouveau type d'élément structural à TSA (ex. un nouveau type de LinearElement ou SurfaceElement) en respectant la hiérarchie Element/LinearElement/SurfaceElement existante.
---

# add-element

Objectif : ajouter un nouveau type d'élément structural sans casser la synchronisation
UI ↔ Model ↔ Geometry ↔ 3D ni dupliquer la hiérarchie existante.

## Procédure

1. **Rechercher la hiérarchie des éléments** — relire `src/Model/Element.h`
   (`Element`, `LinearElement`, `SurfaceElement`) et les implémentations existantes les
   plus proches du nouveau type (`Beam`/`Column`/`TrussMember`/`Cable` pour du linéaire,
   `Slab`/`Wall` pour du surfacique).
2. **Identifier la classe parente** — `LinearElement` ou `SurfaceElement` (ou `Element`
   directement si le nouveau type n'est ni l'un ni l'autre — cas rare, à justifier).
3. **Identifier les propriétés communes** — celles déjà couvertes par l'interface parente
   (`section()`/`material()` pour un `LinearElement`, `thickness()`/`material()` pour un
   `SurfaceElement`).
4. **Identifier les propriétés spécifiques** — ce qui distingue réellement le nouveau type
   (voir `src/Model/Cable/*` pour un exemple d'élément linéaire aux propriétés riches et
   spécifiques : ancrage, précontrainte, normes).
5. **Identifier les sections** applicables — réutiliser `TSA::Model::Section` /
   `SectionShape` existant, ne pas créer un système de section parallèle.
6. **Identifier les matériaux** applicables — réutiliser `Material`/`MaterialLibrary`.
7. **Identifier les coordonnées** — nœuds via `Node`/`Point3D`, niveaux via
   `LevelManager`/`Level` si l'élément est positionné par étage.
8. **Identifier la sélection** — s'assurer que `SelectionManager` pourra référencer le
   nouvel élément par ID, cohérent avec les autres types.
9. **Identifier la géométrie OCCT** — créer un `<NomElement>Geometry.h/.cpp` dans
   `src/Geometry/` suivant le modèle des builders existants (`BeamGeometry`,
   `SlabGeometry`, `WallGeometry`, `FoundationGeometry`, `CableGeometry3D`).
10. **Identifier l'UI** — ajout d'une commande de création (`src/Commands/`, sur le modèle
    de `CreateBeamCommand`), entrée dans le ruban/dock, prise en charge dans
    `PropertyPanel` et `ModelTreeWidget`.
11. **Implémenter** — classe modèle, callbacks `IModelObserver`
    (`onXAdded/Modified/Removed` à ajouter dans l'interface `IModelObserver` de
    `Model.h`), commande de création, geometry builder, intégration `OccView`.
12. **Connecter** — vérifier les deux sens de synchronisation
    (`.agents/rules/06-synchronization.md`) : création, modification, copie, déplacement,
    suppression, undo, redo.
13. **Tester** — ajouter un test dans `tests/` couvrant au minimum la création et le calcul
    des propriétés dérivées (volume/poids/longueur ou aire).
14. **Compiler** — `TSA_Tests` et la cible principale, en Debug puis Release.

## Points de vigilance

- Ne jamais transformer artificiellement le nouveau type en un type existant pour
  « gagner du temps » (un câble ne devient pas une poutre).
- Vérifier que `StructuralClipboard` gère bien le Copy/Paste du nouveau type sans perte de
  type ni de propriétés spécifiques.
