# Repères de la vue 3D — TSA

Affichage des repères global et locaux dans la vue 3D : ce qui est dessiné, d'où viennent les directions, comment le
modifier. Fonction purement graphique : aucun fichier du solveur n'est touché, et le fichier `.tsa` ne change pas.

## 1. Ce que voit l'utilisateur

| Repère | Lettres | Où | Quand |
|---|---|---|---|
| Global | X, Y, Z | à l'origine du modèle | toujours (réglable) |
| Indicateur d'angle et ViewCube | X, Y, Z | coins de la vue | toujours (indicateur réglable) |
| Local d'un élément | x, y, z | centre de l'élément, ou point cliqué | à la sélection |
| Plan de travail | Xwp, Ywp, Zwp | origine du plan | s'il diffère du repère global affiché |

Les axes sont rouge pour x, vert pour y, bleu pour z, avec une taille constante à l'écran. Chaque lettre a la couleur de
son axe, cernée de blanc sur fond sombre et de noir sur fond clair. Les trièdres passent au-dessus de la scène et ne
sont pas sélectionnables.

Le menu **Affichage → Repères** règle : repère global, indicateur d'angle, repères locaux, lettres, position du repère
local, taille des axes (30 à 200 pixels), taille des lettres (8 à 32 pixels). Ces réglages appartiennent à l'application
(`QSettings`, `Display/Axes/*`). Ils ne font pas partie des profils d'affichage ni du projet.

## 2. Origine des directions

Aucune direction n'est recalculée pour l'affichage : `ElementAxisFrames` appelle les fonctions du calcul.

| Élément | Source | Affiché |
|---|---|---|
| Poutre, poteau | `CoordinateTransformationService::computeElementLocalFrame` avec la rotation de section | x, y, z |
| Treillis, câble | même fonction, rotation nulle (convention de `OpenSeesModelMap`) | x seul |
| Dalle | `slabFrame` (U vers le 2e sommet, N de Newell, V = N × U) | x = U, y = V, z = N |
| Voile | `wallSurface` (U le long du voile, V = +Z, N = U × V) | x = U, y = V, z = N |
| Fondation, nœud | aucun repère défini | rien |

- Barre : x va du nœud de départ au nœud d'arrivée. Inverser les nœuds inverse donc x. La référence est Z global, ou
  Y global pour une barre quasi verticale.
- Treillis et câble : la formulation est axiale, seul x a un sens physique. Les axes transverses ne sont pas dessinés.
- Dalle et voile : le repère affiché est celui de la surface. Les efforts de coque sont exprimés dans le repère de
  chaque élément du maillage, qui peut en différer.

## 3. Sélection

La vue suit `SelectionManager::selectionChanged`, émis par tous les chemins : clic, rectangle, arborescence, vidage,
retrait d'un élément supprimé.

- Sélection multiple : un trièdre par élément, jusqu'à 32 (`kMaxElementAxisFrames`). Au-delà, aucun, et la barre d'état
  le signale. Au-delà de ce seuil, les trièdres masqueraient la structure.
- Modification du modèle : le repère suit l'élément déplacé ou tourné (`onModelEdited`, `onModelDiffApplied`).
- Point cliqué : le rayon du clic est relevé à l'appui et rattaché à l'élément si ce clic le sélectionne. Il reste
  valable pour cet élément. Choix par l'arborescence, sélection multiple : centre de l'élément.

## 4. Rendu

| Fichier | Rôle |
|---|---|
| `src/Viewer/ElementAxisFrames.*` | calcul pur des repères, sans OCCT graphique ni état de vue |
| `src/Viewer/AxisTriad.*` | seul dessin d'axes : création, style, placement ; axes du ViewCube |
| `src/Viewer/AxisDisplayStyle.*` | réglages, signal `changed()` |
| `src/Viewer/OccView_Axes.cpp` | méthodes de `OccView` : trièdres de la scène, mise à jour |
| `src/UI/Display/DisplayMenus.*` | menu Repères |

Un seul trièdre (`AIS_Trihedron`) sert partout, en persistance de zoom (`Graphic3d_TMF_ZoomPers`).

- **Performances** : la géométrie est construite une fois, en pixels, autour de l'origine. Placer un trièdre ne change
  que sa rotation et son ancrage. Les trièdres locaux sont réutilisés d'une sélection à l'autre, sans recréation.
- **Placement** : en persistance de zoom, le repère local de l'objet est en pixels. La rotation va dans la transformation
  locale et la position dans le point d'ancrage (`placeAxisTriad`).
- **Sélection OCCT** : affichage avec le mode -1, donc aucun clic ne peut viser un trièdre.

Trois pièges d'OCCT, à connaître avant de modifier ce code :

1. **Ajustement de la vue** : `V3d_View::FitAll` compte le point d'ancrage de tout objet en persistance de zoom, et
   `SetInfiniteState` n'y change rien. Sans précaution, un modèle loin de l'origine serait réduit à un point. D'où
   `OccView::fitScene`, qui efface le repère global et le trièdre du plan le temps du calcul. Tout nouvel appel à
   `FitAll` dans `OccView` doit passer par `fitScene`.
2. **Couleurs** : `Quantity_Color` en RVB est linéaire et sort éclairci par le gamma (0,92 / 0,20 / 0,20 donne du rose).
   `axisScreenColor` relit les valeurs de `AxisColorConfig` en sRVB. Les axes sont aussi dessinés sans éclairage.
3. **Lettres** : contour `Aspect_TODT_DEKALE` et non rectangle de fond, qui exige `core20fwd` (BUG-122).

Captures sans aides à la navigation (`captureViewImage`, vignettes, note de calcul) : repères masqués puis rétablis.

## 5. Vérification

Suite `axes`, tests 541 à 548 (`tests/test_axes.cpp`) ; images rendues écrites dans le dossier `TSA_AXES_IMAGES` si cette
variable est définie.

| Test | Contrôle |
|---|---|
| 541 | 45 barres (horizontales, poteaux, inclinées, 3D quelconques, nœuds inversés ; 0°, 30°, 90°, −45°, 137°) : x, y, z égaux à ceux de `OpenSeesModelMap` à 1e-9, orthonormés, directs |
| 542 | dalle inclinée et voile oblique égaux à `MeshGeometry`, directs ; treillis et câble ; fondation et nœud sans repère ; plafond |
| 543 | point visé par le rayon, bornes de la barre, rayon parallèle |
| 544 | bornes, signal, enregistrement relu (domaine de paramètres isolé) |
| 545 | vue réelle : position et orientation du trièdre, lettres, trièdres réutilisés, sélection multiple, élément déplacé et tourné, réglages, vidage, non sélectionnable |
| 546 | caméra, zoom, modèle à 5 km de l'origine, plan de travail |
| 547 | image rendue, fonds sombre et clair : rouge, vert, bleu, lettres, taille constante (84 puis 85 pixels après un zoom de 4) |
| 548 | clic réel : origine sous le curseur |

## 6. Résultats et limites (2026-10-31, Release, Windows)

| Contrôle | Résultat |
|---|---|
| Suites de `TSA_TestSuite` | 58/58, 555 tests réussis, 0 échec, 1 ignoré |
| TSALab, compilé puis testé après TSA | 27/27 |
| Empreinte des exemples (scripts Tcl et résultats, 104 fichiers) | identique à l'octet |
| Banc officiel | 2 097/2 097, aucun écart |
| `check_layers.py`, `check_shortcuts.py --strict`, `check_translations.py --strict` | conformes |

L'empreinte et le banc ont été mesurés avant les dernières retouches, limitées à `OccView_Navigation.cpp` (appel redondant,
en-têtes) et aux tests. Aucun fichier de `src/Analysis`, `src/Mesh`, `src/Model`, `src/IO`, `src/NDC` ou `src/Standards`
n'est modifié.

Non vérifié :

- le vrai `TSA.exe` : les tests pilotent `OccView`, `SelectionManager` et `DisplayMenus` réels, mais pas `MainWindow`. Le
  message de barre d'état au-delà de 32 éléments, émis par `SelectionSynchronizer`, n'est couvert par aucun test ;
- Debug et Linux ;
- les écrans à haute densité : la taille suit `AnnotationStyle::devicePixelRatio`, sans essai sur un tel écran.

Limites connues :

- `ViewManager::applyStandardView` et `viewNormalToWorkPlane` appellent encore `FitAll` directement. Le seul appelant suit
  aussitôt par `fitScene`, mais tout nouvel appelant devrait faire de même ;
- une exécution de la suite `surfaces` a bloqué à la sortie du processus : BUG-170 dans `.claude/known-issues.md`.
