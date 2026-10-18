# Feuille de route — outils de dessin BIM

Branche `feature/bim-drawing`. Constat : `BIM_DRAWING_AUDIT.md` (problèmes D1 à D13).
Architecture : `BIM_ARCHITECTURE.md` § 5. Bilan : `BIM_DRAWING_IMPLEMENTATION_REPORT.md`.

L'ordre suit celui de la mission. Il est adapté là où l'existant était déjà conforme : modèle
métier, identifiants, accrochage et IFC existaient, avec des tests. Le travail a donc d'abord
porté sur le chemin de création, qui était le maillon défaillant (D1 à D7), puis sur la feuille
de route validée le 18/10/2026.

| # | Étape | Contenu | État |
| :--- | :--- | :--- | :--- |
| 1 | Modèle métier | `Model` + `BimModel` (produit physique ↔ éléments analytiques) | existant, conservé |
| 2 | Identifiants et relations | GlobalId des produits, objets analytiques et types ; mapping 1:N | existant ; vérifié (tests 417, 430) |
| 3 | Commandes transactionnelles | `ElementCreation` : validation, puis nœuds + élément dans une `EditTransaction` | **fait** (tests 410-416, 419) |
| 4 | Accrochage | `SnapEngine` seul moteur de recherche ; coïncidence de nœud centralisée | **fait** ; réglages encore portés par `GridSnapManager` (chantier D) |
| 5 | Outils structuraux | modes Dessin, dialogues Barre / Câble / Surface, six outils avancés, préréglages fondation et treillis | **fait** (tests 410-416, 425) |
| 6 | Palette de propriétés | identité BIM, type et quantités de l'élément | **fait** (tests 422, 426) |
| 7 | Cohérence des vues | branche « Structure BIM » de l'arbre (site, bâtiment, étages, produits, types) | **fait** (test 427) |
| 8 | Sauvegarde et migration | format `.tsa` inchangé ; types en champs facultatifs du bloc BIM ; création des types depuis un projet existant | **fait** (tests 423, 430) ; préréglages non persistés (comme avant) |
| 9 | Adaptateur analytique | modèle analytique de TSA ; résultats périmés à la création et à la propagation d'un type | existant ; vérifié (tests 418, 431) |
| 10 | IFC | éléments dessinés, voiles inclinés, types de produits | **fait** (tests 423, 428, 432) |
| 11 | Quantités | `QuantityTakeoff` dérivé du modèle | **fait** (tests 420, 421, 428) |
| 12 | Voiles à base inclinée | calcul, métré, IFC cohérents | **fait** (tests 424, 428, 429) |
| 13 | Interface réelle | `tools/ui_tests/drawing_scenario.ps1`, 17 vérifications | **fait** |

## Reste à faire

Ordre proposé : E (indépendant, petit), A, D, C, B. B attend que A et C aient stabilisé le contour des
voiles et les références de niveau ; D précède C, car les grilles du modèle doivent s'accrocher par le
service commun et non par un second système.

Hors de ces chantiers :

- **CI (BUG-076)** : correctif préparé dans Setup-Livrable (`scripts/run_tests.ps1`, deux passes, rendu
  logiciel Mesa) et vérifié en local. Il reste à l'exécuter une première fois sur le runner.
- **Test 20 (BUG-079)** : instable sous charge, seuil inchangé.
- **Libellés des matériaux des dialogues** : passer par `MaterialLibrary` plutôt que par une recherche
  par mot-clé.

### A. Éditeur de types BIM

État actuel : un type (`ProductType`, `BimModel`) se modifie seulement par une occurrence, puis par
« Propager » dans l'arbre.

Fichiers :

- nouveau `src/UI/Dialogs/ProductTypeEditorDialog.h/.cpp` ;
- `src/BIM/Core/ProductTypes.h/.cpp`, avec une nouvelle fonction `previewTypeChange` ;
- `src/BIM/Core/BimModel.h/.cpp`, avec `updateType` ;
- `src/UI/ModelTree/ModelTreeWidget.cpp` (entrée « Modifier le type… ») ;
- `src/UI/Properties/ElementBimPanel.cpp` (lien vers le type) ;
- `cmake/TSAProduct.cmake`.

Règles :

- **Propriétés du type** : section et matériau (linéaires) ; épaisseur et matériau (dalles, voiles) ;
  matériau (fondations).
- **Propriétés de l'occurrence**, jamais touchées par le type : géométrie, nœuds, orientation,
  excentrement, relâchements, ouvertures, charges, appuis, nom.
- L'éditeur liste les occurrences et leur écart (`typeDeviation`). La prévisualisation donne, pour
  chaque occurrence, la valeur actuelle et la nouvelle. L'utilisateur choisit la portée : toutes les
  occurrences, seulement celles conformes au type, ou une sélection.
- **Incompatibilités**, qui bloquent la validation et sont expliquées :
  - épaisseur nulle ou négative ;
  - section absente du catalogue et sans géométrie ;
  - matériau sans module ou sans masse volumique ;
  - épaisseur d'un voile inférieure à la profondeur d'une ouverture déclarée ;
  - catégorie du type différente de celle de l'occurrence.
- Une validation = une `EditTransaction` (type et occurrences), une entrée Annuler. Les résultats de
  calcul deviennent périmés (`ResultsValidityGuard`).

Dépendances : `ProductTypes` (fait, tests 430-432), `EditTransaction`, `ResultsValidityGuard`.

Acceptation :

- modifier la section d'un type à 3 occurrences, dont une en écart, avec la portée « conformes » :
  2 occurrences modifiées, l'écart conservé, une seule entrée Annuler ;
- un refus ne change rien ;
- IFC : la nouvelle définition est relue après export.

Tests :

- unitaires sur `previewTypeChange`, la portée, les refus, Annuler / Rétablir, les résultats périmés,
  `.tsa` et IFC ;
- `drawing_scenario.ps1` étendu : ouvrir l'éditeur par l'arbre, modifier, Annuler.

### B. Portes au pied d'un voile à base inclinée

État actuel : refusées (`validateWallOpening`). Le refus reste en place tant que les quatre
consommateurs ne sont pas cohérents.

Constat : le maillage (`MeshGeometry::wallSurface`), le métré (`QuantityTakeoff`), l'IFC
(`IfcGeometryMapper::wallSolid` et ouvertures) et la validation calculent chacun le contour du voile.

Règles à décider et à documenter :

1. **Seuil de la porte** : bas horizontal, à la cote du point le plus haut de la base sous la largeur
   de la porte. Un coin triangulaire de maçonnerie reste sous le seuil, du côté bas.
2. **Hauteur minimale de ce coin**, et à défaut le refus : en dessous, le maillage produirait des
   éléments dégénérés.
3. **Distance minimale** entre la porte et les bords latéraux.
4. **Hauteur de passage** mesurée depuis le seuil, pas depuis la base.

Fichiers :

- nouveau `src/Model/WallOutline.h/.cpp` : un contour unique, c'est-à-dire le parallélogramme moins
  les ouvertures, par découpe de polygones. Il est utilisé par `MeshGeometry.cpp`,
  `QuantityTakeoff.cpp`, `IfcGeometryMapper.cpp` et la validation ;
- `src/Mesh/MeshGeometry.cpp` ;
- `src/BIM/Core/QuantityTakeoff.cpp` ;
- `src/BIM/IFC/IfcGeometryMapper.cpp`, avec un `IfcOpeningElement` prolongé sous la base pour une
  découpe franche ;
- `src/BIM/IFC/IfcImporter.cpp`, pour la relecture.

Dépendances : chantier A conseillé (épaisseur portée par le type), tests 424, 428 et 429 existants.

Acceptation, sur un même voile incliné avec porte :

- surface nette du métré = aire du contour unique ;
- aire maillée = cette même aire ;
- Σ réactions sous poids propre = ρ·g·t·aire nette ;
- volume relu par IfcOpenShell = t·aire nette.

Les cas limites (coin trop bas, porte trop près d'un bord) sont refusés sans rien créer.

Tests : unitaires sur le contour ; calcul en coques sous poids propre ; métré ; IFC validé par
IfcOpenShell ; refus. Essai d'interface : porte ajoutée au voile incliné du scénario.

### C. Grilles BIM persistantes (D12)

État actuel :

- Les grilles (`GridManager`, `GridSystem`, `GridDefinition` : identifiant texte, cartésiennes,
  cylindriques et libres) sont **déjà enregistrées** dans le `.tsa`, dans le chunk GRID en JSON
  (BUG-051).
- Mais elles appartiennent à la session (`ProjectSession`), hors du `Model` : pas d'Annuler commun avec
  les éléments, pas de lien aux étages BIM, aucun export `IfcGrid`. L'import IFC les ignore.

Fichiers :

- `src/Grid/GridDefinition.h/.cpp` (axes nommés : étiquette, position, angle) ;
- `src/Model/Model.h` (grilles du modèle, ou référence au `GridManager` dans les instantanés
  d'Annuler) ;
- `src/BIM/Core/BimModel.h/.cpp` (produit de catégorie `Grid`, déjà déclaré dans `BimTypes.cpp`,
  GlobalId, étage de rattachement) ;
- `src/BIM/IFC/IfcExporter.cpp` (`IfcGrid`, `IfcGridAxis`, placement dans l'étage) ;
- `src/BIM/IFC/IfcImporter.cpp` et `IfcOpenShellImport.cpp` (lecture d'`IfcGrid` au lieu de
  l'ignorer) ;
- `src/IO/TSAFileReader_Chunks.cpp` et `TSAFileWriter_Chunks.cpp` (champs facultatifs : un ancien
  fichier se lit tel quel).

Règles :

- identifiant stable par grille et par axe ;
- nom d'axe unique dans une grille ;
- étage de rattachement facultatif : sans étage, la grille vaut pour tous les niveaux ;
- un nœud posé sur une intersection garde ses coordonnées, sans lien implicite. Un lien
  « nœud ↔ intersection » serait une décision séparée.

Dépendances : chantier D, pour que l'accrochage aux grilles passe par `SnapEngine` seul.

Acceptation :

- grille créée, renommée, puis Annuler ;
- `.tsa` relu, y compris un ancien fichier sans les nouveaux champs ;
- export `IfcGrid` validé par IfcOpenShell, puis réimport avec les mêmes noms d'axes.

Tests : unitaires sur la persistance, la compatibilité (fichiers `examples/*.tsa` du dépôt) et l'IFC ;
non-régression de l'accrochage (suites `grids`, `snap`).

### D. Unification des accrochages (D13)

Correction de l'audit : `GridSnapManager` **a des utilisateurs**.

| Partie | Utilisateurs |
| :--- | :--- |
| Réglages (modes actifs, activation, tolérance) | `OccView` (`activeModes()` transmis à `SnapEngine`, `setSnapEnabled`), `GridSettingsDialog` (cases des modes, tolérance), `ProjectSession` (propriétaire), `MainWindow` (transmission à la vue et au dialogue) |
| Recherche (`findSnap`, `findObjectSnap`) | `SnapManager::findWorkPlaneSnap` seulement, **sans appelant**, plus cinq fichiers de tests (`test_coordinates`, `test_grids`, `test_viewer`, `test_workplane`, `test_common.h`) |

Défauts liés, corrigés le 18/10/2026 (docs/VIEWS_NAVIGATION.md) : l'indicateur « SNAP » de la barre d'état lit
l'état réel de la vue (BUG-078) ; la case « Magnétisme » de `GridSettingsDialog` pilote cet état au lieu de
`GridSnapManager` seul (BUG-080). Il reste à fusionner les réglages (étape 1) et la recherche (étapes 2 à 4).

Étapes, chacune livrable seule et suivie de la suite complète :

1. Extraire les réglages dans un `SnapSettings`, lu par `SnapEngine`, `GridSettingsDialog`, la barre
   d'état et `OccView`.
2. Réécrire les tests qui appellent `findSnap` / `findObjectSnap` sur `SnapEngine`, avec les mêmes
   cas et les mêmes résultats attendus. Les comparer d'abord aux anciens résultats pendant une étape.
3. Supprimer `SnapManager::findWorkPlaneSnap` et la partie recherche de `GridSnapManager`. Garder
   `snapToWorkPlaneGrid`, qui est utilisé par `OccView`.
4. Supprimer `GridSnapManager` une fois sans référence (`cmake/TSAProduct.cmake`).

Acceptation :

- plus qu'un seul chemin de capture ;
- suites `coordinates`, `grids`, `viewer`, `workplane` et `snap` inchangées ;
- l'indicateur suit les commandes Magnétisme et OSNAP, vérifié par un essai UI Automation.

### E. Préréglages persistants

État actuel : `StructurePresets` est porté par `InteractionManager`, remis aux valeurs par défaut à
chaque lancement.

Portée proposée :

- **utilisateur**, dans `QSettings` (`HKCU\Software\TSA Engineering`, déjà utilisé pour le
  presse-papiers) ;
- **projet** en option : l'utilisateur choisit « Enregistrer dans le projet », et le `.tsa` porte un
  bloc facultatif qui l'emporte à l'ouverture.

Pas de portée « installation » : elle demanderait des droits d'écriture que l'installateur sans
administrateur n'a pas.

Fichiers :

- `src/Model/CreationPresets.h`, avec un nouveau `CreationPresets.cpp` (JSON versionné, réutilisant
  les sérialiseurs de section et de matériau de `BimModel.cpp`) ;
- `src/Interaction/InteractionManager.h` ;
- `src/UI/Dialogs/StructurePresetDialog.cpp`, avec un bouton « Rétablir les valeurs par défaut » ;
- `src/UI/MainWindow.cpp` (chargement au démarrage, enregistrement à la validation du dialogue) ;
- `src/IO/TSAFileWriter_Chunks.cpp` et `TSAFileReader_Chunks.cpp`, pour la portée projet.

Règles :

- une valeur absente ou invalide (section ou matériau inconnu de la bibliothèque) revient à la
  valeur par défaut, avec un message ;
- un projet sans bloc garde le comportement actuel ;
- les valeurs par défaut du code restent la référence.

Acceptation : préréglage modifié, TSA relancé, valeur retrouvée ; « Rétablir » rend les valeurs par
défaut ; ancien projet ouvert sans changement.

Tests : unitaires sur l'aller-retour JSON, les valeurs invalides et l'ancien projet. L'essai
d'interface sauvegarde et restaure déjà `HKCU\Software\TSA Engineering`, ce qui permet de vérifier la
relecture après relance.
