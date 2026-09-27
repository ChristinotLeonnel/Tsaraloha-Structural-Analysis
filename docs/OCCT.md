# OCCT & 3D — TSA

> Document vivant. Voir aussi `.agents/rules/04-occt-3d.md`.

## Composants réels

### `src/Viewer/OccView`

`class OccView : public QWidget, public TSA::Model::IModelObserver` — widget hôte du
viewer OCCT. Détient/utilise :

- `AIS_InteractiveContext`, `V3d_View`, `V3d_Viewer`, `OpenGl_GraphicDriver`,
  `Aspect_DisplayConnection` — infrastructure de rendu OCCT.
- `AIS_ViewCube` — cube de navigation.
- `AIS_RubberBand` — sélection rectangle.
- `Graphic3d_ClipPlane` — plans de coupe (outil de visualisation uniquement, ne modifie
  jamais le modèle).
- `TSA::Grid::GridManager` / `GridSnapManager` (via `GridRenderer.h`) — grille et
  accrochage.
- `TSA::Model::CreationPresets`, `TSA::Interaction::InteractionManager` — création et
  interaction.
- `MaterialVisual` — apparence visuelle (indépendante des propriétés mécaniques du
  `Material` du modèle métier).

En implémentant `IModelObserver`, `OccView` est directement notifié de tout changement du
modèle (`onBeamAdded`, `onBeamModified`, `onBeamRemoved`, et l'équivalent pour chaque type
d'élément) — c'est le point d'entrée réel de la synchronisation Model → 3D.

### `src/Viewer/SelectionManager`

Gestion de la sélection 3D — doit rester traçable jusqu'à des IDs d'éléments/nœuds du
modèle.

### `src/Viewer/MaterialVisual`, `src/Viewer/TextureManager`

Apparence visuelle (couleur, rugosité, texture) — voir `VisualProperties` dans
`src/Model/Material.h` pour les propriétés visuelles réellement stockées au niveau du
modèle (`baseColor`, `roughness`, `metallic`, `transparency`, `shininess`, `textureName`,
`texturePath`, `textureScaleU/V`).

### `src/Geometry/*Geometry` (Geometry Builders)

`BeamGeometry`, `SlabGeometry`, `WallGeometry`, `FoundationGeometry`, `CableGeometry3D` —
construisent les `TopoDS_Shape` à partir des paramètres du modèle (section, matériau,
nœuds/coordonnées). `TODO: VERIFY IN SOURCE` pour l'absence éventuelle d'un
`ColumnGeometry`/`TrussMemberGeometry` dédié — vérifier si `Column`/`TrussMember`
réutilisent `BeamGeometry`.

### `src/Grid`

`GridManager`, `GridSnapManager`, `GridRenderer` — grilles 3D paramétriques et
accrochage, cohérentes avec `TSA::Coordinate::CoordinateSystem`.

### `src/Interaction/InteractionManager`

Gestion des interactions utilisateur dans le viewport 3D. `TODO: VERIFY IN SOURCE` pour
le détail exact des évènements gérés.

## Flux obligatoire

```text
Model → Paramètres (Section/Material/Node) → Geometry Builder → OCCT Shape → AIS → OccView
```

Ne jamais construire une `Shape` directement depuis des valeurs UI.

## Performances

Éviter les redraw/reconstructions globales de la scène : ne régénérer que la géométrie de
l'élément modifié, via son *Geometry Builder* dédié et son callback `IModelObserver`
correspondant.

## Copie d'éléments

La géométrie d'un élément copié (`StructuralClipboard`) doit être **recréée** depuis le
modèle copié, jamais partagée avec la `Shape` OCCT source.

## Vérification

`TODO: VERIFY IN SOURCE` pour le détail interne de chaque Geometry Builder — lire les
`.cpp` correspondants avant modification.
