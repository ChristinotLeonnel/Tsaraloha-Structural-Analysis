# Représentations du modèle — TSA

Accès : ruban **Affichage > Représentation**, ou menu **Affichage > Représentation du modèle**. Pas de
raccourci clavier : volontairement non attribué (commandes déclarées dans le catalogue sans raccourci). Le mode choisi est
conservé pendant la session (y compris à l'ouverture d'un autre projet) ; il n'est pas enregistré dans le
fichier `.tsa`.

Les quatre modes sont **purement visuels** : aucun ne modifie le modèle, les sections, les matériaux, les
appuis, les charges, les résultats, leur révision, le maillage ou les identifiants (tests 252 et vérification
GUI : résultats identiques et toujours « à jour » après plusieurs changements de mode).

| Mode | Barres | Dalles, voiles, fondations | Surcouche |
| :--- | :--- | :--- | :--- |
| Modèle physique | sections volumiques, matériaux | solides | — |
| Filaire analytique | axe nœud à nœud (poutres bleues, poteaux rouges, treillis verts, câbles orange) | arêtes seules | — |
| Éléments finis | axes analytiques | arêtes seules | maillage du solveur (dernier calcul à jour) |
| Superposition | sections translucides | solides translucides | axes analytiques par-dessus |

Nœuds, appuis, charges, étiquettes de nœuds, déformée et diagrammes restent affichés dans tous les modes.

## Comportement

- **Même objet OCCT** : en filaire, l'`AIS_Shape` de chaque barre reçoit son axe au lieu de sa section ; il
  reste enregistré dans le `SelectionManager`. Sélection, surbrillance, isolation, propriétés et accrochage
  (calculé sur le modèle) sont inchangés. Une barre de rôle « Poteau » s'affiche comme un poteau
  (`analyticalKindOf`).
- **Profondeur** : axes et arêtes dans le calque `Top` (dessinés après les lignes de grille confondues, sans
  alternance de pointillés, mais masqués par ce qui est devant) ; axes superposés et maillage dans le calque
  `Topmost`, non sélectionnables.
- **Coût** (Debug, 900 barres) : Physique ↔ Superposition ≈ 20 ms (transparence seule), Filaire ↔ Éléments
  finis ≈ 15 ms, vers le filaire ≈ 100-150 ms, retour au physique ≈ 1,8 s (reconstruction des solides,
  comme à l'ouverture du projet).

## Maillage du solveur

Trois représentations distinctes : géométrie physique, modèle analytique (axes et nœuds structuraux), maillage
numérique. Le mode Éléments finis affiche **uniquement** le maillage joint aux résultats par le moteur
(`ResultsModel::solverMesh`, structure neutre `TSA::Analysis::SolverMesh`) :

| Moteur | Source | Contenu actuel |
| :--- | :--- | :--- |
| OpenSees | `OpenSeesModelMap::solverMesh`, la correspondance qui écrit `model.tcl` | un élément fini par barre TSA (elasticBeamColumn, truss, corotTruss), nœuds du modèle ; ressorts : nœud auxiliaire fixé + zeroLength |
| Custom2D | `Custom2D::solverMesh`, l'entrée réellement passée au solveur plan | un élément Frame / Truss par barre de la portée, nœuds aux coordonnées 3D réelles |

Aucun moteur actuel ne subdivise les barres ni ne maille dalles et voiles (BUG-002) : l'affichage l'indique
(« 1 élément fini par élément TSA (aucune subdivision) », « Dalles et voiles : non maillés par ce moteur »).
Rien n'est inventé : sans calcul, avec des résultats obsolètes ou un moteur qui ne fournit pas son maillage,
seuls les axes sont affichés et un message explicite apparaît (barre d'état et console).

Tracé (`FiniteElementMeshRenderer`) : arêtes des éléments en vert-jaune (lignes, arêtes des triangles,
quadrangles, tétraèdres, hexaèdres), nœuds issus du modèle en cercles blancs, nœuds propres au moteur
(subdivisions, auxiliaires) en croix orange ; éléments d'une barre masquée (isolation, filtres) non tracés.

**Intégrer un futur moteur** : construire un `SolverMesh` à partir de ce que le moteur reçoit réellement (un
`SolverMeshCell` par élément fini, `source` = élément TSA d'origine, nœuds internes avec `tsaNodeId = 0`),
puis `results.setSolverMesh(mesh)` dans `AnalysisEngine::run`. Aucune autre modification n'est nécessaire.

## Architecture

| Couche | Fichier | Rôle |
| :--- | :--- | :--- |
| calcul | `src/Analysis/SolverMesh.*` | maillage neutre, résumé, cohérence |
| vue | `src/Viewer/ModelDisplayMode.*` | modes, politique (`DisplayPolicy`), axes, couleurs, état et géométrie du maillage — sans OpenGL, testés |
| vue | `src/Viewer/DisplayModeRenderers.*` | `AnalyticalModelRenderer` (axes superposés), `FiniteElementMeshRenderer` |
| vue | `src/Viewer/OccView_DisplayMode.cpp` | changement de mode, styles des barres et surfaces, surcouches différées |
| interface | `src/UI/MainWindow_DisplayMode.cpp` | actions exclusives, messages ; ruban `RibbonBuilder::buildViewTab` |

## Tests

Suite `displaymodes` (tests 250-256) : structure `SolverMesh`, maillage OpenSees identique à
`OpenSeesModelMap` (sans exécuter le solveur), maillage joint à un vrai calcul OpenSees et à un vrai calcul
Custom2D, messages (sans calcul, obsolète, moteur sans maillage, dalles non maillées), géométrie tracée
(subdivisions, surfaces, volumes, masquage), politique des modes, axes et familles.
