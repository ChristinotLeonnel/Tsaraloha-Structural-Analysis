# Vues 2D / 3D, ViewCube, plans de travail et vues nommées

Ce document fixe les conventions de navigation de TSA et décrit le comportement livré. Il consigne aussi
l'audit et la finalisation du 18/10/2026, puis la correction de la grille du plan de travail et la navigation sur la
grille structurelle (même jour, § 2 « Synchronisation » et « Navigation »).

## 1. Conventions

| Sujet | Choix retenu |
| :--- | :--- |
| Axe vertical | **Z** (génie civil). Le ViewCube est en mode Z vertical (`SetYup(false)`). |
| Unités | Mètre, repère global. |
| Direction de caméra | De l'œil vers la cible ; droite de l'écran = direction ^ haut. |
| Vues | Déterministes : la même vue quelle que soit la vue de départ. |
| Projection des faces et des plans 2D | Orthographique. |
| Projection en 3D | Celle de la caméra : orthographique par défaut dans OCCT. Le retour du mode 2D rend celle d'avant. |
| Précision | Directions égales à 1e-9. Point cliqué dans le plan à moins de 1e-9 m. Nœuds jamais modifiés par une vue (égalité exacte). |

| Vue | Direction | Haut de l'écran | Droite de l'écran | Plan de travail |
| :--- | :--- | :--- | :--- | :--- |
| Dessus | (0, 0, -1) | +Y | +X | horizontal au niveau actif |
| Dessous | (0, 0, +1) | -Y | +X | horizontal au niveau actif |
| Avant | (0, +1, 0) | +Z | +X | XZ (normale -Y) |
| Arrière | (0, -1, 0) | +Z | -X | XZ |
| Gauche | (+1, 0, 0) | +Z | -Y | YZ (normale +X) |
| Droite | (-1, 0, 0) | +Z | +Y | YZ |
| Isométrique | (-1, +1, -1)/√3 | Z rendu orthogonal | — | inchangé |
| Arête, coin du cube | opposée à l'axe OCCT (`V3d::GetProjAxis`) | Z rendu orthogonal | — | inchangé |
| Face au plan | -N du plan (œil côté +N) | axe V du plan | U | le plan |

Les plans globaux vus de face donnent exactement ces vues : XY = dessus, XZ = avant, YZ = droite.

## 2. Comportement

### Vues 2D (Plan XY, Plan XZ, Plan YZ)

Un vrai plan 2D se compose de cinq éléments :

- le plan de travail de la face : niveau actif pour le dessus et le dessous, dernier plan XZ ou YZ utilisé pour les
  élévations ;
- la projection orthographique ;
- une caméra perpendiculaire au plan ;
- l'orbite bloquée : le bouton droit fait un panoramique, et le zoom n'incline jamais la vue ;
- l'isolation non destructive des éléments du plan.

En 2D, une face (ViewCube, commandes `TOP`, `BACK`…) change de plan 2D. Une arête, un coin ou l'isométrie ramènent en
3D. Un plan déjà face à la vue est conservé (niveau, coupe nommée). Un plan incliné est vu selon sa normale, avec son
axe V vers le haut.

### Retour en 3D

« Vue 3D » rend la caméra et la projection d'avant le mode 2D. Après historique ou restauration d'une caméra, le mode
2D n'est jamais conservé avec une caméra inclinée (`keep2DConsistent`).

### Grille du plan de travail

`WorkPlaneGrid` dessine des lignes aux multiples du pas d'accrochage du plan (`WorkPlane::gridSpacingX/Y`, 1 m par
défaut), dans le repère local du plan. Elle est portée par le panneau du plan et suit ses déplacements.

- **Densité** : pas affiché = pas du plan × 1, 2, 5, 10, 20, 50…, le plus petit gardant au moins 12 pixels entre deux
  lignes, avec au plus 400 lignes par direction. Une ligne sur cinq est accentuée.
- **Redessin** : la grille est recalculée à chaque changement de caméra, mais redessinée seulement si le pas change.
  Il n'y a ni scintillement ni coût à chaque mouvement.
- **Étendue** : la taille du plan (20 m × 20 m par défaut) est un minimum. Si le modèle projeté dans le plan en dépasse,
  le panneau et sa grille sont agrandis et recentrés sur l'emprise du modèle.
- **Lignes** : elles restent aux multiples du pas depuis l'origine du plan.
- **Visibilité** : masquable par plan (`WorkPlane::setIsGridVisible`), sinon suit la visibilité du plan. Couleurs selon
  le thème clair ou sombre.
- **Interactions** : non sélectionnable, sans effet sur la sélection et l'accrochage aux objets.

**Grille de travail et grille structurelle** sont deux choses distinctes :

| | Grille de travail | Grille structurelle |
| :--- | :--- | :--- |
| Objet | `WorkPlaneGrid`, portée par le plan de travail | `GridSystem` du `GridManager` (cartésienne, cylindrique, arbitraire) |
| Pas | libre, réglé par plan (`gridSpacingX/Y`) | positions réelles des axes, quelconques |
| Rôle | dessin et accrochage au pas | références : niveaux, axes, intersections |
| Suit | le plan de travail | le modèle |

### Synchronisation du plan de travail

Un seul chemin met à jour tout ce qui dépend du plan : `OccView::syncWorkPlaneDependents(trsf)`. Il est emprunté par
les commandes (`setActiveWorkPlane` → `applyWorkPlaneTransformation`) et, à chaque mouvement, par le glisser du
manipulateur 3D (`syncWorkPlaneFromManipulator`). Dans l'ordre :

1. grille de travail (`WorkPlaneGrid::SetLocalTransformation`) ;
2. trièdre du plan ;
3. plan privilégié du visualiseur ;
4. service de coordonnées (`CoordinateTransformationService`), lu par l'accrochage et la saisie ;
5. niveau actif et surbrillance du niveau (plan horizontal) ;
6. isolation des éléments du plan ;
7. redessin immédiat.

Puis `workPlaneChanged` est émis. La fenêtre met à jour la barre d'état, le panneau des propriétés, le volet
Projection et le sélecteur de niveau, ce dernier **sans réappliquer le plan** (`ViewportContainer::reflectActivePlane`).

- **Translation au manipulateur** : le type du plan est conservé (XY reste XY, décalage = altitude). Les axes ne sont
  modifiés, et le plan rendu « personnalisé », que si l'orientation change (rotation).
- **Échap pendant un glisser** : le plan, sa grille et le manipulateur reviennent à leur position de départ, que la
  touche arrive à la vue ou au raccourci « Mode sélection » de la fenêtre. Rien n'est validé au relâchement.
- **Caméra** : orbite, panoramique et zoom ne changent ni le plan ni la grille (seule la densité affichée de la grille
  dépend du zoom).
- **Console** : une ligne « Plan de travail actif » par changement ; pendant un glisser, seule la position finale.

### Navigation sur la grille structurelle

Les références sont calculées à partir des intersections réelles des grilles structurelles visibles
(`TSA::Grid::WorkPlaneReferences`, couche modèle, sans dépendance à la vue). Aucun pas régulier n'est supposé et aucune
coordonnée n'est écrite en dur : des axes à 0, 2, 3, 5, 6 et 8 m donnent exactement ces plans.

- **Plans parallèles** : les intersections sont regroupées selon leur distance au plan actif (le long de sa normale).
  Chaque groupe est un plan parallèle. Cela vaut pour un plan incliné et une grille non orthogonale.
- **Identifiant ≠ coordonnée** : un plan est nommé d'après la famille d'axes constante dans son groupe et
  discriminante dans la grille (« niveau R+1 », « axe 5 »). L'axe « 5 » peut être à X = 6 m.
- **Intersections du plan** : celles situées dans le plan (à 1e-6 m près), ordonnées par (v, u) dans son repère local.
- **Plus proche** : intersection du plan la plus proche du dernier point visé par le curseur.

| Commande | Raccourci | Ligne de commande | Menu Affichage › Plans de Travail |
| :--- | :--- | :--- | :--- |
| Plan parallèle suivant / précédent | Alt+PgUp / Alt+PgDown | `WPNEXT` / `WPPREV` | oui |
| Intersection suivante / précédente | Ctrl+Alt+PgUp / Ctrl+Alt+PgDown | `WPINEXT` / `WPIPREV` | oui |
| Origine sur l'intersection la plus proche du curseur | Alt+Origine | `WPALIGN` | oui |
| Plan précédent (historique, 50 plans) | Alt+Retour arrière | `WPBACK` | oui |
| Déplacer le plan de dx dy dz (m) | — | `WPMOVE dx dy dz` | — |
| Origine du plan en x y z (m) | — | `WPORIGIN x y z` | — |
| Plans globaux XY, XZ, YZ, étage | — | — | oui (existants) |
| Plan personnalisé (3 points, face) | — | — | oui (dialogue existant) |
| Vue normale au plan | Num 0 | `WPNORMAL` | — (existant) |

Les raccourcis sont dans le catalogue (`docs/shortcut.txt`, sans conflit). Chaque commande annonce le résultat dans la
console et la barre d'état (référence, origine), ou « Plan de travail inchangé » avec la raison. L'infobulle de
l'indicateur du plan (barre d'état) donne nom, origine, normale et pas de la grille de travail.

### Accrochage

- **Grille du plan** : on accroche ce que l'on voit, au pas affiché. La tolérance est proportionnelle au pas (0,5 m
  pour le pas de 1 m).
- **Mode 2D** : seuls les points situés à moins de 5 cm du plan sont accrochables, et les objets masqués par
  l'isolation ne le sont pas. Un point accroché hors du plan est projeté selon la normale ; il devient un point « le
  plus proche », jamais le nœud lui-même.
- **Indicateur « SNAP »** de la barre d'état (BUG-078 corrigé) : il lit l'état réel de la vue (`isGridSnapEnabled`,
  `isObjectSnapEnabled`) et affiche « ACTIF », « GRILLE », « OBJETS » ou « INACTIF ». Il est mis à jour par les signaux
  de la vue, quelle que soit l'origine du changement : ruban, commandes `SNAP` / `OSNAP`, raccourcis, dialogue des
  grilles.
- **Dialogue des réglages de grille** : sa case « Magnétisme » pilote désormais cet état réel. Elle ne modifiait
  auparavant qu'une copie que le calcul d'accrochage ne lisait pas.

### ViewCube et transitions

- **Faces, arêtes, coins** : orientations exactes du tableau.
- **Clic** : traité à l'appui ; son relâchement ne sélectionne rien. Auparavant, le relâchement relançait l'animation
  interne d'OCCT sur la face devenue visible sous le curseur.
- **Transition** : toutes les orientations demandées sont animées (faces, arêtes, coins, isométrie, plans 2D,
  orientation sur le plan, vues nommées).
  - L'animation est réglée sur le temps écoulé : 180 ms, adoucie au départ et à l'arrivée.
  - Une image lente (grand modèle) écourte l'animation sans l'allonger.
  - Elle se termine toujours par la caméra finale exacte, et ne passe jamais par une perspective vers une vue
    orthographique.

### Vues nommées

Elles se gèrent dans le dialogue « Vues nommées… » (ruban Affichage, menu Affichage › Projections, commandes `VIEWS` /
`VNAMED`) :

- enregistrer la vue actuelle ;
- restaurer (ou double-clic) ;
- renommer (un nom déjà pris est refusé) ;
- supprimer.

Une vue nommée mémorise :

- la caméra : œil, cible, haut, échelle et projection ;
- le mode 2D et la face du plan 2D ;
- le plan de travail.

À la restauration, ces éléments sont rétablis exactement, avec transition.

**Persistance** : les vues appartiennent au projet. Elles sont enregistrées dans le `.tsa`, chunk `VIEW`, format 1.12
(voir `TSA_FILE_FORMAT.md`).

- Un ancien projet s'ouvre sans vue nommée.
- Une version antérieure de TSA ignore ce chunk inconnu, qu'elle sait sauter.
- Chaque document a son modèle, donc ses vues : deux projets ne se mélangent pas.
- Enregistrer une vue marque le projet à enregistrer, sans changer sa révision. Les résultats de calcul restent valides.

**Capture de la caméra** : en orthographique, OCCT peut ramener l'œil sur la cible (distance nulle). La capture
recalcule l'œil depuis la direction. Cela corrige aussi la caméra mémorisée pour l'aperçu des projets récents
(`cameraState`), qui pouvait être refusée à la restauration.

### Commandes

| Commande | Ruban (Affichage › Projections) | Menu Affichage › Projections | Ligne de commande | Raccourci (catalogue) |
| :--- | :--- | :--- | :--- | :--- |
| Vue 3D | oui (grand bouton) | oui | — | configurable |
| Plan XY / XZ / YZ (2D) | oui | oui | — | configurables |
| Dessus, Face, Droite, Gauche | oui | oui | `TOP`, `FRONT`, `RIGHT`, `LEFT` | Num 7, Num 1, Ctrl+Num 3, Num 3 |
| Dessous, Arrière | — | oui | `BOTTOM`, `BACK` | Ctrl+Num 7, Ctrl+Num 1 |
| Isométrique, Accueil | oui | oui | `ISO`, `HOME` | Num 5, Origine |
| Normale au plan | Affichage (grand bouton) | — | `WPNORMAL` / `VPN` | Num 0 |
| Vues nommées | oui | oui | `VIEWS` / `VNAMED` | — |
| ViewCube | faces, arêtes, coins | — | — | — |

Aucune commande de vue n'a été retirée du ruban : le panneau Projections contient la vue 3D, les trois plans 2D,
Dessus, Face, Droite, Gauche, Isométrique, Accueil et Vues nommées.

### Vues multiples

TSA n'a pas d'architecture multi-fenêtres de vue : une `OccView` par document, sans vue partagée. Une simulation
superficielle n'a pas été ajoutée.

Le service de navigation et les vues nommées sont prêts à servir plusieurs vues. Il reste à développer :

- plusieurs `V3d_View` sur un même `V3d_Viewer`, ou plusieurs `OccView` sur un même modèle ;
- une vue active pour les commandes ;
- une mise en page (2×2, plan + 3D) ;
- la synchronisation de la sélection.

## 3. Audit et corrections

| # | Défaut | Cause | Correction |
| :--- | :--- | :--- | :--- |
| B1 | « Plan XY / XZ / YZ » : caméra seulement tournée, pas de mode 2D. | `setViewPlaneMode` ne faisait que `SetProj`. | `showPlanView` : plan de la face, mode 2D, caméra exacte. |
| B2 | Sortie du mode 2D en perspective. | État « orthographique » initialisé à faux. | Projection lue sur la caméra. |
| B3 | Orientation sur le plan dépendant de la vue de départ. | `SetUp` avant `SetProj`. | `ViewManager::applyFrame` (œil, cible, haut ensemble). |
| B4 | Mode 2D avec caméra inclinée (cube, dock, historique). | Chemins de caméra sans connaissance du 2D. | Service unique, `keep2DConsistent`. |
| B5 | Arête / coin du cube : face voisine. | Clic traité à l'appui et au relâchement. | Clic consommé à l'appui. |
| B6 | Plan de travail vu par la tranche en 3D. | Plan indépendant de la vue. | Plan de la face à la place. |
| B7 | Vue affichée non annoncée. | Textes figés des actions. | Barre d'état et console d'après l'état réel. |
| BUG-078 | « SNAP: ACTIF » jamais mis à jour. | Étiquette créée une fois. | Indicateur relié à l'état réel. |
| B8 | Case « Magnétisme » du dialogue des grilles sans effet. | Elle modifiait `GridSnapManager`, non lu par l'accrochage. | Pilote l'état de la vue. |
| B9 | Panneau du plan toujours de 20 m × 20 m autour de l'origine. | Taille par défaut explicite : le calcul « dynamique » n'était jamais exécuté. | Taille = minimum, agrandie et recentrée sur le modèle. |
| B10 | Caméra capturée avec l'œil sur la cible. | Distance nulle en orthographique. | Œil recalculé depuis la direction. |
| B11 (BUG-083) | Grille fantôme : plan déplacé au manipulateur, grille restée sur l'ancien plan. | Le glisser ne transformait que le panneau et le trièdre, pas `WorkPlaneGrid` ni le niveau actif. | Chemin unique `syncWorkPlaneDependents`, à chaque mouvement. |
| B12 (BUG-084) | Translation au manipulateur : plan XY devenu « personnalisé ». | `setLocalAxes` impose le type `Custom`. | Axes changés seulement si l'orientation change. |
| B13 (BUG-085) | Échap pendant un glisser : plan déplacé conservé ; dans l'application, la touche était sans effet. | L'annulation ne rétablissait que le panneau ; `setInteractionMode(Select)` sortait aussitôt, le mode Sélection étant déjà actif. | Plan et grille rétablis ; annulation aussi par le raccourci « Mode sélection ». |
| B14 (BUG-086) | Plan d'étage déplacé en X ou Y ramené à l'origine globale ; une entrée « Z = … m » ajoutée au sélecteur à chaque mouvement. | Écho vers le sélecteur de niveau, qui réappliquait `setWorkPlaneAxisAndOffset`. | `reflectActivePlane` (affichage seulement) ; entrée personnalisée réutilisée. |
| B15 (BUG-087) | Panneau du plan non sélectionnable après l'ouverture d'un projet : manipulateur inaccessible. | `rebuildAllShapes` vidait le registre de sélection, panneau compris. | Panneau réenregistré. |
| B16 | Plan horizontal personnalisé : niveau actif non mis à jour. | Seuls les types globaux étaient traités. | Niveau actif = altitude du plan. |
| B17 (BUG-088) | Console : une ligne « Plan de travail actif » par mouvement de souris pendant un glisser. | `workPlaneChanged` émis à chaque mouvement. | Ligne écrite au relâchement seulement. |

Pas de quadrillage : supprimé en septembre (texture défectueuse), il est remplacé par `WorkPlaneGrid`, lignes
vectorielles avec décalage de profondeur du panneau.

## 4. Tests

Suite `views`, tests 433 à 448. Résultats du 18/10/2026, en plateforme Qt native :

| Test | Contenu | Résultat |
| :--- | :--- | :--- |
| 434 | Orientations calculées (tableau, 26 orientations du cube, plans inclinés). Tourne aussi sous `offscreen`. | réussi |
| 439 | Pas affiché de la grille, vues nommées en JSON (lecture tolérante), `.tsa` 1.12 relu à l'identique, ancien projet sans vue, révision inchangée. Tourne aussi sous `offscreen`. | réussi |
| 433 | Non-régression B1 à B4. | réussi |
| 435 | Vues standard depuis 4 départs, 26 orientations du cube avec transition, vrai clic d'arête, mode 2D synchronisé, historique, orbite bloquée, zoom. | réussi |
| 436 | Dessin par vrais clics : plan d'étage (Z exact), élévations XZ et YZ ; accrochage à un nœud ; Annuler / Rétablir. | réussi |
| 437 | Projet du dépôt (380 nœuds) : géométrie identique au bit près après toutes les vues, conversion écran ↔ plan, grille couvrant tous les nœuds. | réussi |
| 438 | Plans de travail adaptés à la vue, plans nommés conservés, plan incliné en 2D. | réussi |
| 440 | Grille dans la vue : pas multiple du pas d'accrochage, 12 pixels minimum, dézoom et zoom, grille portée par le niveau et l'élévation, masquage, accrochage au pas affiché (Z exact). | réussi |
| 441 | Vues nommées : restauration exacte depuis un autre plan, vue 3D libre, vue invalide refusée ; dialogue (enregistrer, renommer, supprimer, restaurer) ; projets séparés. | réussi |
| 442 | Dessin vu de dessous, en élévations arrière et gauche, sur deux niveaux ; dalle et voile sur plan incliné à 30° (nœuds dans le plan à 1e-9 m) ; géométrie intacte. | réussi |
| 443 | Références structurelles sans vue : axes irréguliers 0, 2, 3, 5, 6, 8 (libellés ≠ coordonnées), niveaux, 18 intersections, plus proche, plan incliné. Tourne aussi sous `offscreen`. | réussi |
| 444 | Manipulateur, vrais événements souris : grille qui suit pendant et après le glisser, une seule grille, type XY conservé ; Échap (touche et raccourci « Mode sélection ») ; plan précédent. | réussi |
| 445 | Déplacements X puis Y, accrochage au pas depuis la nouvelle origine, plan tourné, caméra (orbite, panoramique, zoom) sans effet sur plan et grille. | réussi |
| 446 | Navigation dans la vue sur la grille irrégulière : coupes à 2, 3, 5, 6, 8 m, axe « 5 » à X = 6 m, niveau R+1, intersections, alignement, plan précédent, autre document intact. | réussi |
| 447 | Écho vers le sélecteur de niveau (câblage de la fenêtre) : origine dans le plan conservée, une seule entrée personnalisée. | réussi |
| 448 | Panneau du plan sélectionnable au clic après reconstruction des formes. | réussi |

Les tests 444, 447 et 448 échouent sur le code d'avant leur correction (vérifié en réintroduisant chaque défaut).

Sous `offscreen`, les tests 433, 435 à 438, 440 à 442 et 444 à 448 sont annoncés sautés, jamais comptés réussis. Dans
la CI de Setup-Livrable, la suite passe en plateforme native.

Essais de l'interface réelle (voir `tools/ui_tests/README.md`) :

- `views_scenario.ps1`, **19 vérifications** (essai 11) ;
- `workplane_scenario.ps1`, **9 vérifications** (essai 12) : `WPMOVE`, `WPBACK`, `WPNEXT` / `WPPREV`, `WPINEXT` sur la
  grille du projet R+5, puis vrai glisser de la poignée Z du manipulateur (Échap, glisser complet, `WPBACK`). Cet essai
  a trouvé les défauts B13 (touche Échap), B14, B15 et B17, invisibles aux tests sans fenêtre principale.

## 5. Limites

- Vues multiples : non développées (voir § 2).
- Plan de travail incliné et ViewCube : une face du cube quitte le plan incliné pour le plan global de la face.
- Les plans de travail inclinés ne se créent pas par l'essai d'interface : le dialogue des plans personnalisés n'est pas
  piloté. Le dessin sur plan incliné est vérifié par les tests 438 et 442.
- Pas de grille polaire ni de grille personnalisée par plan au-delà du pas en U et en V.
- Le nom d'un plan déplacé n'est pas mis à jour : un plan nommé « RDC (Z=0.00) » garde ce nom à 15 m. L'origine et le
  niveau affichés, eux, sont justes.
- Sous 1 920 px de large, l'indicateur « Plan: … » de la barre d'état est masqué faute de place (`fitStatusBar`,
  comportement existant). L'origine reste lisible dans la console et le sélecteur de niveau.
- La navigation par intersections ne pilote pas encore les grilles cylindriques et arbitraires par l'essai d'interface :
  elle est vérifiée sur grille cartésienne (tests 443, 446 et essai 12) ; les autres types passent par le même calcul
  (`intersections()` de chaque grille).
