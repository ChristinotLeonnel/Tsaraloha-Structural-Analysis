---
title: OCCT & 3D
scope: repo
applies_to: ["src/Viewer/**", "src/Geometry/**", "src/Grid/**", "src/Interaction/**"]
---

# 04 — OCCT & 3D

## Composants réels

- `src/Viewer/OccView` : `QWidget` **et** `TSA::Model::IModelObserver` — c'est le widget
  hôte du viewer OCCT (`AIS_InteractiveContext`, `V3d_View`, `V3d_Viewer`,
  `OpenGl_GraphicDriver`, `Aspect_DisplayConnection`). Il reçoit aussi
  `AIS_ViewCube`, `AIS_RubberBand`, `Graphic3d_ClipPlane` (plans de coupe).
- `src/Viewer/SelectionManager` : gestion de la sélection 3D.
- `src/Viewer/MaterialVisual`, `src/Viewer/TextureManager` : apparence visuelle
  (indépendante des propriétés métier du `Material` du modèle — ne pas confondre les deux).
- `src/Geometry/{Beam,SlabWall,Foundation,CableGeometry3D}...Geometry` : construction des
  `TopoDS_Shape`/`AIS_Shape` à partir des paramètres du modèle (Geometry Builders).
- `src/Grid` : `GridManager`, `GridSnapManager`, `GridRenderer` (grilles 3D et accrochage).
- `src/Interaction/InteractionManager` : interactions utilisateur dans le viewport.

## Flux obligatoire

```text
Model → Paramètres (Section/Material/Node/coordonnées) → Geometry Builder (src/Geometry) → OCCT Shape → AIS → OccView
```

Ne jamais construire une `TopoDS_Shape`/`AIS_Shape` directement depuis des valeurs UI : elle
doit toujours provenir des paramètres réellement stockés dans le modèle.

## Performances

- Éviter les redraw/reconstructions globales de la scène pour une modification localisée :
  ne régénérer que la géométrie de l'élément modifié (via son *Geometry Builder* dédié) et
  mettre à jour son `AIS_Shape` correspondant dans `OccView`.
- Les callbacks `IModelObserver` (`onBeamModified`, etc.) sont le point d'entrée attendu
  pour ce genre de mise à jour ciblée.

## Copie d'éléments

Lorsqu'un élément est copié (`src/Model/StructuralClipboard`), sa géométrie 3D doit être
**recréée depuis son modèle structural copié**, jamais réutilisée telle quelle depuis la
`Shape` OCCT source — sous peine de faire pointer deux éléments logiquement distincts vers
la même représentation graphique (et de perdre leur indépendance lors d'une future édition).

## Sélection et clip planes

- La sélection OCCT doit toujours être traçable jusqu'à un ID d'élément/nœud du modèle
  (voir `SelectionManager`), pas seulement jusqu'à une `Shape` anonyme.
- Les plans de coupe (`Graphic3d_ClipPlane`) sont un outil de visualisation : ils ne
  modifient jamais le modèle structural sous-jacent.

## Responsabilité et modularité de la vue 3D (`OccView`)

- `OccView` est un composant de **présentation visuelle et d'interaction 3D**.
- Ne pas y concentrer :
  - des calculs métier ou normatifs (ex. Eurocodes) ;
  - des calculs géométriques complexes qui appartiennent à `CoordinateTransformationService`
    ou aux `*Geometry` builders ;
  - la logique de commande ou d'annulation (qui appartient à `src/Commands` et `src/UndoRedo`).
- Pour les manipulations interactives 3D (Gizmo, déplacement temporaire de plan de travail ou d'éléments) :
  - Privilégier les transformations locales d'affichage (`SetLocalTransformation`) pour éviter
    tout re-maillage ou reconstruction géométrique lourde pendant le drag de la souris.
  - La reconstruction géométrique complète n'intervient qu'à la validation finale de l'opération.

## Vérification

`TODO: VERIFY IN SOURCE` pour le détail interne de chaque `*Geometry` builder et pour la
liste exacte des évènements gérés par `InteractionManager` — lire les `.cpp` correspondants
avant modification.
