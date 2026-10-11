# Visualisation du maillage FEM dans la vue 3D de TSA

Ce document décrit comment le maillage produit par Gmsh (docs/MESH.md) est affiché, inspecté et coloré par les
résultats dans la vue 3D de TSA. Il s'adresse aux développeurs (architecture, choix techniques, limites) et aux
utilisateurs (commandes).

## 1. Audit et choix du moteur graphique

| Élément audité | Constat |
|---|---|
| Qt / compilateur / construction | Qt 6.11.2, MSVC (Visual Studio 18), C++20, CMake + Ninja (préréglages `ninja-debug`, `ninja-release`) |
| Moteur 3D existant | OpenCASCADE 8.0.1 : `V3d_Viewer` / `V3d_View` / `AIS_InteractiveContext` / `OpenGl_GraphicDriver` |
| Vue | `OccView` (QWidget, contexte OpenGL natif d'OCCT créé au premier affichage) : caméra, cube de vue, grille, plan de travail, accrochage, sélection du modèle (`SelectionManager`), coupe (`Graphic3d_ClipPlane`) |
| Données Gmsh | `GmshMeshEngine` → `FeMesh` (nœuds, éléments, groupes) → `MeshProject` (chunk MESH du `.tsa`) |
| Affichage précédent | `MeshOverlay` : un tableau de segments (arêtes de coins), sans faces, sans sélection, sans étiquettes, sans résultats |
| VTK | `3rdparty-vc14-64/vtk-9.4.1-x64` présent mais compilé **sans Qt** (ni `vtkGUISupportQt` ni `QVTKOpenGLNativeWidget`) |
| Modules OCCT disponibles | `TKMeshVS.dll` (MeshVS : visualisation de maillages) livré avec le SDK, non encore lié |
| Déploiement Windows | dépendances résolues par la table d'import (`RuntimeDependencies.cmake`), `windeployqt`, `verify_package.py` |

**Décision : réutiliser le moteur existant (OCCT) avec MeshVS. VTK n'est pas retenu pour la vue de modélisation.**

- La vue de modélisation est un contexte OCCT unique. Y intégrer VTK imposerait soit un second contexte OpenGL dans
  la même fenêtre (caméras, sélection et accrochage dédoublés), soit le remplacement d'`OccView`, c'est-à-dire la
  réécriture de toute la modélisation. C'est la stratégie la plus risquée.
- Le VTK disponible n'a pas de module Qt : il faudrait recompiler VTK contre Qt 6.11 et ajouter environ 100 Mo de
  DLL au paquet.
- MeshVS, livré avec OCCT, fournit faces, volumes, liens 1D, nœuds, étiquettes, couleurs et sélection par entité dans
  **le même** `AIS_InteractiveContext` : la caméra, le cadrage, l'accrochage et la sélection du modèle sont
  conservés sans modification.
- VTK reste envisageable pour une future fenêtre de post-traitement séparée : le modèle d'affichage et les champs de
  résultats (§3, §6) ne dépendent d'aucun moteur.

## 2. Architecture

```
FeMesh (Gmsh → TSA : identifiants TSA, étiquettes Gmsh, entités, groupes physiques)
   │
   ▼
MeshDisplayModel (src/Mesh, sans moteur graphique)
   topologies Gmsh, filtres (1D / 2D / 3D, groupes isolés, coupe), faces extérieures / intérieures des volumes,
   arêtes uniques, correspondances identifiant ↔ indice, erreurs de conversion
   │
   ├──► MeshVSDataSource (src/Viewer) ──► MeshVS_Mesh : faces, volumes, liens, nœuds, étiquettes, sélection
   ├──► calque des arêtes uniques (un seul tableau de segments)
   ├──► calques au premier plan : qualité insuffisante (rouge), entité mise en évidence (jaune)
   └──► calque des résultats (couleurs par sommet) + AIS_ColorScale (légende)
                ▲
MeshResultField / MeshResultAdapter (src/Mesh) ◄── ResultsModelMeshAdapter ◄── ResultsModel (OpenSees, Custom2D)

MeshVisual (src/Viewer) : orchestre le tout dans le contexte d'OccView
MainWindow_MeshView.cpp (src/UI) : commandes, inspection, sélection, résultats, animation
```

| Fichier | Rôle |
|---|---|
| `src/Mesh/MeshDisplayModel.h/.cpp` | modèle d'affichage indépendant du moteur |
| `src/Mesh/MeshResultField.h/.cpp` | champs de résultats sur le maillage, adaptateurs de solveurs |
| `src/Viewer/MeshVSDataSource.h/.cpp` | `MeshVS_DataSource` lue dans le modèle d'affichage |
| `src/Viewer/MeshVisual.h/.cpp` | affichage, modes, étiquettes, sélection, résultats, cadrage (remplace `MeshOverlay`) |
| `src/Viewer/OccView*` | crochet de clic et de survol pour les calques (`setOverlayPickHandlers`), `fitBox` |
| `src/UI/MainWindow_MeshView.cpp` | menu « Affichage du maillage », dock « Inspection du maillage » |
| `src/UI/Ribbon/RibbonBuilder.cpp` | bouton « Affichage du maillage » (onglet Analyse, groupe Discrétisation) |

Le calque ne modifie jamais le modèle ni le maillage : il affiche une **copie** du `FeMesh`. Une régénération, une
fermeture ou une ouverture de projet ne laisse donc aucune référence pendante.

**Contexte OCCT différé.** `OccView` ne crée son contexte qu'à son premier affichage, alors que `MainWindow`
construit le calque avant ce moment. `MeshVisual` lit donc le contexte et la vue à chaque usage. L'ancien
`MeshOverlay` gardait un contexte nul et n'affichait rien dans l'application réelle. Le test 346 et l'essai
d'interface (§9) couvrent ce cas.

## 3. Correspondance Gmsh → TSA → affichage

- **Identifiants.** Les identifiants MeshVS sont les identifiants métier `FeNode::id` / `FeElement::id`, jamais des
  indices. `MeshDisplayModel` construit ses tables identifiant → indice sans supposer des identifiants consécutifs,
  croissants ou égaux aux étiquettes Gmsh. Les tests utilisent volontairement des identifiants épars et des éléments
  en ordre inverse.
- **Étiquettes Gmsh.** Elles sont conservées à la génération et à l'import :
  - `FeNode::gmshTag` ;
  - `FeElement::gmshTag`, `gmshEntityDim` et `gmshEntityTag` ;
  - `FeGroup::gmshPhysicalDim` et `gmshPhysicalTag`.

  Elles sont enregistrées dans le projet sous forme de tableaux parallèles facultatifs (`gmshNodeTags`,
  `gmshElements`, `gmshPhysical`). Un projet antérieur se relit avec des étiquettes inconnues (0).
- **Groupes.** À l'import, une entité membre de plusieurs groupes physiques figure dans chacun d'eux. Les entités sans
  groupe forment des groupes `importé:d-t`.
- **Erreurs de conversion.** Un élément est écarté de l'affichage et listé dans « Inspection du maillage » dans les cas
  suivants : type non pris en charge, nombre de nœuds différent de celui attendu, nœud inexistant, identifiant
  dupliqué, ou type inconnu relu dans un projet. Un type inconnu est marqué dans la source (`type inconnu « … »`) et
  n'est jamais converti en un autre type ; l'adaptateur de calcul le bloque également.

### Types d'éléments pris en charge (ordre des nœuds de Gmsh)

| Type | Nœuds | Affichage |
|---|---|---|
| Line2 | 2 | lien MeshVS |
| Line3 | 3 | lien entre les extrémités ; nœud milieu affiché comme nœud ; résultats sur la ligne brisée |
| Tri3, Quad4 | 3, 4 | face |
| Tri6, Quad8 | 6, 8 | face polygonale passant par les nœuds milieux |
| Tet4, Hex8 | 4, 8 | volume : faces affichées |
| Tet10, Hex20 | 10, 20 | volume : faces à 6 ou 8 sommets (milieux) |

Aucun autre type n'est deviné.

## 4. Modes d'affichage

- **Filaire** : arêtes uniques seules.
- **Faces et arêtes** : mode par défaut.
- **Faces seules.**
- **Éléments réduits** : contrôle de la connectivité.
- **Faces transparentes** (50 %).
- **Nœuds** du maillage, avec une taille réglable.
- **Épaisseur des traits** réglable.
- **Numéros des nœuds et des éléments** : au plus 1000 de chaque. Au-delà, un sous-ensemble réparti sur toute la liste
  est affiché et l'inspection le signale.
- **Filtres** : éléments 1D, 2D et 3D ; isolation de groupes (objets maillés, groupes physiques) ; restauration de
  tous les groupes.
- **Volumes.** Seules les faces extérieures sont affichées : une face est intérieure si deux volumes affichés la
  partagent. « Faces intérieures des volumes » les affiche toutes, pour l'inspection.
- **Coupe du maillage.** Les éléments dont le centre est au-delà d'un plan normal à X, Y ou Z sont masqués. Les faces
  des volumes exposées par la coupe sont recalculées et deviennent extérieures : la coupe montre de vraies faces
  d'éléments, sans surface ouverte.
- **Arêtes sans doublon.** Une arête partagée par plusieurs éléments n'est tracée qu'une fois. MeshVS ne trace aucune
  arête de face, sauf en mode réduit.
- **Géométrie.** « Afficher le maillage » superpose maillage et géométrie. « Maillage seul » masque barres, dalles,
  voiles, semelles et nœuds du modèle, puis rétablit leur état précédent. Décocher « Afficher le maillage » laisse la
  géométrie seule.
- **Qualité.** Les éléments dont le minSICN est inférieur à 0,1 sont signalés en rouge, au premier plan. Un clic dans
  « Qualité du maillage » met l'élément en évidence et cadre la vue dessus.

## 5. Sélection, propriétés, cadrage

« Sélection dans le maillage » active les modes de sélection MeshVS : nœud, lien, face et volume. L'objet est
d'abord enregistré auprès du gestionnaire de sélection (`Load`), sans quoi l'activation est sans effet.

Hors de ce mode, le maillage n'est pas détectable et la sélection du modèle reste inchangée.

Le crochet d'`OccView` propose au calque **toutes** les entités détectées sous le curseur. Le plan de travail,
confondu avec une dalle maillée, est en effet souvent détecté en premier. Quand le calque traite un clic, la
sélection du modèle est vidée.

Le survol affiche « Maillage : nœud … » ou « Maillage : élément … ». Les propriétés s'affichent dans « Inspection du
maillage » :

- **élément** : type, ordre, nœuds, étiquette et entité Gmsh, source, groupes, qualité ;
- **nœud** : coordonnées, nœud TSA confondu, étiquette Gmsh, source, groupes, valeur du champ ou « non calculé ».

« Cadrer sur le maillage » utilise `OccView::fitBox` : historique de caméra, profondeur, règles mises à jour.

## 6. Résultats

L'architecture des résultats est séparée du maillage et ouverte à plusieurs solveurs :

- un `MeshResultField` porte un nom, une unité, une source (solveur et version), une méthode, une localisation
  (nœud ou élément) et des valeurs indexées par **identifiant métier** ;
- chaque solveur fournit ses champs par un `MeshResultAdapter`.

`ResultsModelMeshAdapter` (moteurs de barres OpenSees et Custom2D) calcule le déplacement nodal ainsi :

- **nœud confondu avec un nœud TSA calculé** : valeur exacte du solveur ;
- **nœud intérieur d'une barre maillée** : interpolation linéaire entre les deux stations encadrantes du solveur
  (positions le long de la barre), puis passage du repère local au repère global (`LoadResolver::localVectorToGlobal`).
  Un nœud hors de l'axe n'est jamais deviné ;
- **nœuds des dalles et des semelles** : aucune valeur, car TSA ne transmet ni coques ni volumes au solveur. La note
  le précise et ces nœuds restent gris.

Résultats absents ou obsolètes : aucun champ.

« Déplacements sur le maillage » affiche :

- les couleurs de la norme du déplacement, avec une légende `AIS_ColorScale` à 10 classes portant le nom, l'unité et
  le solveur ;
- la forme déformée (facteur automatique : 5 % de la dimension du maillage, ou facteur saisi) ;
- la forme initiale en arêtes grises.

« Animer la déformée » fait osciller le facteur. La cadence suit la durée de construction de l'affichage, ce qui évite
toute accumulation d'images en retard.

## 7. Performances

Les mesures suivantes proviennent du test 345, sur un bloc de 108 000 hexaèdres (115 351 nœuds, 14 400 faces
extérieures, 28 800 arêtes uniques), avec une vue réelle de 900 × 600 :

| Étape | Debug | Release |
|---|---|---|
| Modèle d'affichage (faces extérieures, arêtes uniques) | 1,7 s | 0,45 s |
| Affichage OCCT (MeshVS + arêtes) | 2,2 s | 0,50 s |
| Première détection après activation de la sélection | 0,59 s | 0,45 s |

Le maillage réel de l'essai d'interface (317 nœuds, 694 éléments 1D, 2D et 3D) se construit en 4 à 16 ms.

Plusieurs mécanismes limitent le coût :

- seules les faces extérieures des volumes sont transmises à MeshVS ;
- un volume entièrement intérieur n'est ni tracé ni sélectionnable ;
- les arêtes forment un seul tableau de segments ;
- les étiquettes sont limitées en nombre ;
- la sélection n'est calculée qu'en mode sélection.

## 8. Construction et déploiement

- **CMake** : `TKMeshVS` est ajouté à `OCCT_LIBS`. Aucune autre dépendance n'est ajoutée : MeshVS fait partie
  d'OpenCASCADE, distribué sous LGPL 2.1 avec exception, déjà livré avec TSA.
- **Paquet** : `TKMeshVS.dll` est copié automatiquement par la résolution des imports. `verify_package.py` valide le
  paquet : 77 binaires, toutes les importations satisfaites, manifeste conforme.
- **Résolution des dépendances** : `RuntimeDependencies.cmake` accepte désormais une même DLL trouvée à deux endroits
  (copie déjà déployée et original du SDK), à condition que les copies soient identiques (SHA-256). Sinon, la
  construction échoue.
- **Poste propre** : l'exécutable du paquet a été lancé avec un `PATH` réduit à Windows, sans Qt, OCCT ni Gmsh.
  L'essai d'interface complet (§9) y obtient 34/34.

## 9. Tests

**Suite `meshview`** (tests 335 à 346, `TSA_TestSuite --suite=meshview`) :

- topologies et orientation des faces ;
- maillage mixte 1D, 2D et 3D à identifiants épars ;
- arêtes uniques : 2 triangles → 5 arêtes ; bloc 2×2×2 → 48 arêtes, 54 avec les faces intérieures ;
- faces des volumes : bloc 2×2×2 → 24 extérieures et 12 intérieures ; deux tétraèdres → 6 et 1 ;
- source MeshVS ; erreurs de conversion ; groupes, filtres et coupe ;
- génération Gmsh réelle avec étiquettes et relecture JSON ;
- import `.msh` à étiquettes non consécutives, avec une entité dans deux groupes physiques ;
- champs de résultats : valeurs exactes, interpolées ou absentes ;
- affichage dans une `OccView` réelle : modes, étiquettes, résultats, légende, effacement ;
- sélection par la détection OCCT et par un vrai clic souris envoyé à `OccView` ;
- grand maillage ;
- 15 cycles de 3 régénérations ;
- calque créé avant l'initialisation de la vue.

**Essai de l'application réelle** (`tools/ui_tests/mesh_view_scenario.ps1`, projet écrit par
`TSA_TEST_DUMP_DIR=<dossier> TSA_TestSuite --suite=meshview`). Le scénario :

- ouvre le projet et génère le maillage par le ruban ;
- relit les comptes du maillage affiché dans « Inspection du maillage » ;
- passe en revue les quatre modes, « maillage seul », les étiquettes et le filtre de dimension ;
- sélectionne une entité par un clic réel dans la vue 3D ;
- redimensionne la fenêtre ;
- lance le calcul (F5) et affiche les déplacements sur le maillage ;
- enregistre, rouvre (maillage relu à l'identique), puis enchaîne 3 cycles ouverture / fermeture ;
- vérifie le journal de session (aucune erreur).

Les commandes du menu déroulant du ruban ne sont pas exposées à UI Automation par Qt : le scénario les parcourt au
clavier (table de positions identique à `populateMeshViewMenu`).

## 10. Limites

- Line3 est tracé comme un segment entre ses extrémités. Les barres de TSA sont droites, et le calque des résultats
  suit la ligne brisée qui passe par le nœud milieu.
- Les faces du second ordre sont des polygones passant par les nœuds milieux, pas des surfaces courbes.
- Les couleurs sont interpolées par sommet : entre un nœud calculé et un nœud sans valeur (gris), la transition est
  un dégradé, alors que la légende est en classes.
- Seuls les déplacements des barres sont disponibles : les surfaces et les volumes n'ont pas de résultats, car aucun
  solveur ne les calcule dans TSA. Les contraintes et efforts par élément sont prévus par l'architecture
  (`FieldLocation::Element`) mais aucun adaptateur ne les fournit encore.
- Depuis l'interface, la coupe se fait par élément selon X, Y ou Z. L'API accepte une normale quelconque. Il n'y a pas
  de coupe OpenGL avec remplissage.
- L'animation reconstruit l'affichage à chaque image ; sa cadence ralentit sur un très grand maillage.
- Le maillage affiché est une copie : la mémoire occupée par le maillage est doublée pendant l'affichage.
- En mode sélection, les entités du maillage sont prioritaires au clic sur celles du modèle.
