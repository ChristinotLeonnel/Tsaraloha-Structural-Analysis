# Vues 2D / 3D, ViewCube et plans de travail

Branche `feature/views-workplanes`. Ce document fixe les conventions de navigation de TSA, consigne l'audit
du 18/10/2026 et décrit le service de navigation qui en est issu.

## 1. Conventions

| Sujet | Choix retenu |
| :--- | :--- |
| Axe vertical | **Z** (génie civil). Le ViewCube est en mode Z vertical (`SetYup(false)`). |
| Unités | Mètre, repère global. |
| Direction de caméra | De l'œil vers la cible ; droite de l'écran = direction ^ haut. |
| Vues | Déterministes : la même vue quelle que soit la vue de départ. |
| Projection des faces | Orthographique. |
| Projection en 3D | Celle de la caméra : orthographique par défaut dans OCCT. Le retour du mode 2D rend celle d'avant. |
| Précision | Égalité des directions à 1e-9. Point cliqué dans le plan à moins de 1e-9 m. Nœuds jamais modifiés par une vue (égalité exacte). |

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
Le test 434 le vérifie.

**Accrochage en mode 2D** (règle existante, conservée) :
- seuls les points situés à moins de 5 cm du plan sont accrochables ;
- les objets masqués par l'isolation du plan ne le sont pas ;
- un point accroché hors du plan est projeté selon la normale. Il devient alors un point « le plus proche », et non le
  nœud lui-même : aucun nœud hors plan n'est relié implicitement.

## 2. Audit du 18/10/2026

Les défauts ont été reproduits dans une vraie vue OCCT avant correction : test 433, sur le code d'origine.

| # | Défaut constaté | Cause | Fichiers | Correction | Test |
| :--- | :--- | :--- | :--- | :--- | :--- |
| B1 | « Plan XY / XZ / YZ » ne donnaient qu'une caméra tournée : pas de mode 2D, plan de travail inchangé, aucun bouton visible dans le ruban. | `setViewPlaneMode` ne faisait que `SetProj`. Le vrai mode 2D (`setMode2D`) était séparé et utilisait le plan actif quel qu'il soit. | `OccView_Navigation.cpp`, `RibbonBuilder.cpp` | `showPlanView` : plan de la face, mode 2D, caméra exacte. Boutons « Plan XY / XZ / YZ » dans le ruban et le menu. | 433, 436 ; essai UI |
| B2 | Sortie du mode 2D : la vue 3D passait en **perspective**. | `ViewManager::m_isOrthographic` démarrait à `false` alors que la caméra OCCT est orthographique ; `setMode2D(false)` « restaurait » cette valeur. | `ViewManager.h/.cpp`, `OccView_Navigation.cpp` | Projection lue sur la caméra (`cameraIsOrthographic`). | 433 |
| B3 | « Orienter sur le plan » : le haut de l'écran dépendait de la vue de départ, par exemple un plan incliné vu de travers depuis l'isométrie. | `SetUp` avant `SetProj` : OCCT remplace silencieusement un « haut » parallèle à la direction *courante*. | `ViewManager.cpp`, `OccView_Navigation.cpp` | `ViewManager::applyFrame` : œil, cible et haut posés ensemble. | 433, 434 |
| B4 | Mode 2D avec une caméra non perpendiculaire au plan, après un coin du ViewCube, l'historique ou un état restauré. | Plusieurs chemins modifiaient la caméra sans connaître le mode 2D (ViewCube, dock de projection, vue précédente). | `OccView_Events.cpp`, `OccView_Navigation.cpp` | Service unique. Arête, coin ou isométrie quittent le 2D ; `keep2DConsistent` après historique ou restauration. | 433, 435 |
| B5 | Clic sur une arête ou un coin du ViewCube : vue d'une **face voisine** au lieu de l'arête (constaté dans l'application réelle). | Clic traité deux fois : à l'appui, puis au relâchement, où la sélection appelait `HandleMouseClick` du cube sur la face devenue visible sous le curseur. Le défaut existait avant ce chantier. | `OccView_Events.cpp` | Clic du cube entièrement consommé par l'appui : le relâchement ne sélectionne rien. | 435 (vrai clic) ; essai UI |
| B6 | En 3D, vue avant avec un plan de travail horizontal : plan vu par la tranche, donc points cliqués hors du plan affiché (repli sur Y = 0). | Plan de travail indépendant de la vue. | `OccView_Navigation.cpp` | Plan vu par la tranche remplacé par le plan de la face. Un plan déjà face à la vue est conservé (niveau, coupe nommée). | 438 |
| B7 | La vue affichée n'était pas annoncée après un clic sur le ViewCube ; les actions affichaient un texte figé (« Vue Dessus » même en plan 2D). | Texte posé par chaque action, pas par l'état réel. | `MainWindow.cpp` | `onNavigationViewChanged` : barre d'état et console « Vue : … » d'après le service. | essai UI |

## 3. Architecture

- **`ViewManager`** (existant, étendu) est la seule source des orientations : `standardFrame`,
  `frameForOrientation` (26 orientations du cube), `planeFrame` (plan quelconque, base U, V, N reconstruite
  orthonormée) et `applyFrame` (application atomique).
- **`OccView`** expose le service de navigation, seul point d'entrée des changements d'orientation :

  | Fonction | Rôle |
  | :--- | :--- |
  | `showStandardView` | Faces et isométrie. En 2D, une face change de plan 2D. |
  | `showPlanView` | Vrai plan 2D d'une face. |
  | `show3DView` | Retour en 3D. |
  | `showCubeOrientation` | ViewCube. |

  `viewTop()`…`viewRight()`, `setViewPlaneMode`, `setViewOrientation`, `applyStandardView` (dock de projection) et le
  clic du ViewCube passent tous par lui. En dehors de ce service, seuls la navigation à la souris (orbite en 3D,
  panoramique, zoom), la rotation dans le plan (`rotate2D`, direction conservée) et la restauration d'une caméra
  (historique, aperçu du projet récent, suivie de `keep2DConsistent`) touchent à la caméra. Vérifié par recherche des
  appels `SetProj`, `SetUp`, `SetEye` et `Camera()->Copy`.
- **Mode 2D** : projection orthographique, caméra perpendiculaire (`orient2DView`), orbite bloquée (bouton droit =
  panoramique), isolation non destructive des éléments du plan. L'orientation suit la face choisie si le plan lui est
  parallèle, sinon la normale du plan (plan incliné).
- **Plans de travail** : `workPlaneForFace`. Dessus et dessous donnent le niveau actif ; les élévations, le dernier plan
  XZ ou YZ utilisé. Un plan déjà face à la vue est conservé.
- **Transition** : courte interpolation (`Graphic3d_Camera::Interpolate`, 180 ms) pour les arêtes et coins du cube.
  Elle se termine toujours par la copie exacte de la caméra finale.
- **Persistance** : le format `.tsa` n'est pas modifié. La caméra n'est mémorisée que dans les métadonnées d'aperçu des
  projets récents, comme avant.

## 4. Tests

Suite `views`, tests 433 à 438 :

| Test | Contenu |
| :--- | :--- |
| 434 | Orientations calculées. Tourne aussi sous `offscreen`. |
| 433 | Non-régression B1 à B4. |
| 435 | Vues standard depuis 4 départs, 26 orientations du cube, vrai clic sur une arête, mode 2D synchronisé, historique, orbite bloquée, zoom. |
| 436 | Dessin par vrais clics dans le plan d'étage (Z = 3 m exactement), l'élévation XZ et l'élévation YZ ; accrochage au nœud existant ; Annuler / Rétablir ; géométrie intacte. |
| 437 | Projet `.tsa` du dépôt (11/10/2026) : 380 nœuds identiques au bit près après toutes les vues ; conversion écran ↔ plan à 1,5 pixel près. |
| 438 | Plan vu par la tranche remplacé, plans nommés conservés, plan incliné à 30° en mode 2D avec un point cliqué dans le plan. |

Les tests 433 et 435 à 438 demandent une vraie fenêtre : sous `offscreen`, ils sont annoncés sautés et jamais comptés
réussis. Dans la CI de Setup-Livrable, la suite est ajoutée à la passe en plateforme native.

Essai de l'interface réelle : `tools/ui_tests/views_scenario.ps1`, 12 vérifications (voir `tools/ui_tests/README.md`,
essai 11).

## 5. Limites

- Pas de vues enregistrées nommées ni de vues multiples : le service est prêt à les servir, mais aucune interface
  n'existe.
- Les faces du ViewCube changent de vue sans transition ; seuls les arêtes et coins sont interpolés.
- La grille du plan de travail est un rectangle teinté, sans quadrillage. La grille structurelle (axes, niveaux) reste
  en 3D et se lit en élévation par projection.
- L'indicateur « SNAP: ACTIF » de la barre d'état n'est toujours pas relié à l'état réel (BUG-078).
- Plans de travail inclinés : dessin vérifié pour les poutres. Les autres outils (dalles, voiles) suivent le même
  pointage, mais n'ont pas de test dédié dans un plan incliné.
