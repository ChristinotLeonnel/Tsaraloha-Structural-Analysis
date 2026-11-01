# Repères locaux, rotation de section et charges : alignement sur Robot Structural Analysis

Date : 2026-10-31. Branche : `refactor/load-axes-section-rotation`, créée depuis `fix/copy-paste-complete`.

## 1. Résultat

- **Section** : TSA reproduit maintenant le repère local et l'angle de section γ de Robot Structural Analysis. C'est vérifié
  en création, rotation, symétrie, copies multiples et inversion des nœuds, sur 92 barres copiées mesurées dans Robot.
- **Charges** : une charge n'a plus que des axes, X, Y, Z globaux ou x, y, z locaux. Le concept de direction « gravité »
  est retiré du modèle, des moteurs et des fenêtres.
- **Copies** : une charge globale garde sa direction ; une charge locale suit l'élément.
- **Anciens projets** : ils sont convertis à la lecture, sans changer l'action physique.
- **Calcul** : sur 7 cas de charge et 5 consoles à section tournée, les réactions de TSA sont égales à celles de Robot à
  1e-12 près (§ 6).

## 2. Mesures dans Robot

Robot Structural Analysis Professional 2024 (24.0) est installé sur le poste. Je l'ai piloté par son API COM, invisible,
sans enregistrer de fichier, et fermé sans sauvegarde. Les mesures sont figées dans
`tests/data/robot_rsa2024_reference.json` ; les scripts sont décrits au § 8.

### Repère local à γ = 0

| Barre | x | y | z |
|---|---|---|---|
| non verticale | de i vers j | Z global × x, horizontal | x × y |
| verticale | ±Z | **+Y global, quel que soit le sens de x** | x × y |

- **Seuil de verticalité** : une barre est verticale si l'écart horizontal rapporté à sa longueur est inférieur à 1e-3.
  Il a été mesuré par pas, entre 6,7e-4 et 1,67e-3, sur trois directions et dans les deux sens.
- **γ** : rotation de y et z autour de x, règle de la main droite. Le signe a été vérifié sur des γ positifs et négatifs.

### Ce que fait Robot lors d'une transformation

| Opération | Repère de la copie | γ |
|---|---|---|
| Translation | identique | inchangé |
| Rotation (copie ou déplacement) | image rigide du repère : y' = image de y | recalculé (ex. poteau, rotation Z 90° : 30° → 120°) |
| Symétrie (copie ou déplacement) | x' et z' images de x et z ; **y' opposé à l'image de y** (repère direct) | recalculé (ex. poutre : 30° → −30° ; poteau, plan x = 50 : 30° → 150°) |
| Inversion des nœuds | x et y inversés, z inchangé | −γ |

La rotation, mesurée autour de Z, X et Y, a donné le même résultat sur toutes les barres, y compris en copies répétées.
La symétrie, mesurée sur les plans X, Y, Z et le plan diagonal x = y, aussi.

**Les deux règles « β » ou « −β » imposées d'avance seraient fausses.** Robot conserve le repère physique, et γ en découle.
Pour une poutre horizontale en symétrie, cela donne −γ. Pour un poteau, cela donne 180° − γ ou d'autres valeurs.

### Charges dans Robot

Les enregistrements de charge copiés gardent exactement leurs valeurs, y compris l'indicateur « local ».

- **Charge globale** : sa direction est fixe.
- **Charge locale** : elle suit le repère de la copie défini ci-dessus.
- **Poids propre (charge « Dead »)** : c'est un vecteur dans le repère global (X, Y, Z), pas une direction spéciale.

## 3. État de TSA avant correction

| Point | TSA | Robot |
|---|---|---|
| y d'une barre verticale, γ = 0 | +X (Y × x) | +Y |
| Seuil de verticalité | 2,56° (\|x·Z\| > 0,999) | sin 1e-3 |
| γ après rotation ou symétrie | inchangé | recalculé (§ 2) |
| Direction « gravité » | −Z, signe saisi ignoré, traitement à part dans `LoadResolver` et `Custom2DAdapter` | aucune |
| Axe local saisi en repère global, ou l'inverse | lu comme une gravité | impossible |
| Charge surfacique | gravité ou normale locale −N | axes globaux ou locaux |
| Copie : charges | images transformées (travail précédent) | enregistrements recopiés |

Les fichiers dupliquant la construction du repère sont maintenant alignés :
`CoordinateTransformationService::computeElementLocalFrame` pour le calcul, `BeamGeometry::sectionFrame` pour le dessin,
`IfcGeometryMapper::barFrame` pour l'IFC. OpenSees, Fem3D et Custom2D reçoivent tous le repère du premier.

## 4. Modifications

### Repère et rotation de section

- `CoordinateTransformationService` : convention de Robot pour les barres verticales et seuil
  `kVerticalTolerance = 1e-3`, exposé par `isVertical`. Aux multiples de 90°, les composantes sont exactes.
- `CopyTransform::barRotationImage` calcule γ de la copie selon les règles mesurées. Il est appliqué par
  `StructuralClipboard` (Ctrl+V, Copier, réseaux, rotation, symétrie), par `Model::transformNodes` (rotation sans copie)
  et par `Model::mirrorElements` (symétrie sans copie).

### Charges

- `LoadDirection` = { GlobalX, GlobalY, GlobalZ, LocalX, LocalY, LocalZ }. Les valeurs stockées sont inchangées ; la
  valeur 3 (gravité) n'est plus utilisée.
- `MemberLoad` : le repère n'est plus un champ, il découle de l'axe (`coordSystem()`). Une incohérence axe / repère
  devient impossible.
- `SurfaceLoadDirection` = { GlobalX, GlobalY, GlobalZ, LocalX (U), LocalY (V), LocalZ (N) }.
- `LoadResolver` : une seule règle, axe de la charge et q signé. `usesMagnitudeOnly` et les cas « |q| » sont retirés, comme
  le refus des moments répartis « sans axe ». Plus aucune charge ne peut être sans axe.
- Copie : les enregistrements sont recopiés tels quels. Seule la position en (u, v) d'une charge sur une dalle copiée en
  miroir est convertie, car le repère de la dalle change.

### Interface

- `UI/Common/LoadAxisCombo` : un même choix « Repère : Global (X, Y, Z) / Local (x, y, z) » puis « Axe ». Il est utilisé
  dans les charges sur barre, les propriétés d'une charge et les charges surfaciques. Les axes sont affichés en rouge,
  vert et bleu, comme les trièdres de la vue 3D.
- Les options et textes propres à la gravité sont retirés. Une charge descendante s'écrit Z négatif.
- La commande d'automatisation `loads.add_uniform` prend désormais q suivant Z global (q < 0 vers le bas).

### Compatibilité et migration

| Donnée ancienne | Conversion à la lecture (`LegacyLoads`, `LegacyBarFrames`) |
|---|---|
| Charge sur barre « gravité » | Z global, intensité −\|q\| (même action : l'ancien signe était ignoré) |
| Axe local en repère global | Z global, −\|q\| (ce que lisaient les moteurs) |
| Axe global en repère local | z local, −\|q\| (ce que lisaient les moteurs) |
| Charge surfacique « gravity » ou sans direction | Z global, valeur et gradients opposés |
| Charge surfacique « normal » (−N) | z local, valeur et gradients opposés |
| Poutre ou poteau vertical (ancien seuil), `.tsa` de format < 3, `.tsa3d` < 1.1 | γ recalculé pour garder les mêmes axes y et z physiques |
| Treillis ou câble vertical (sans γ) | charges locales réexprimées dans le nouveau repère |

- **Formats** : `.tsa` passe en version 3.0, `.tsa3d` en 1.1 ; ils sont lus par toutes les versions.
- **Interprétation** : aucune ancienne charge n'est interprétée comme locale sans raison. La conversion reproduit
  exactement ce que calculaient les moteurs.
- **Banc de validation** : la direction absente vaut « gravité », comme dans un ancien fichier. Deux poteaux de RDM-06
  ont été convertis.

### Poids propre

La génération automatique du poids propre (`includeSelfWeight`, `LoadCase::selfWeightFactor`) est conservée. Elle produit
une force suivant −Z global, c'est-à-dire une composante ordinaire, sans direction spéciale, comme la charge « Dead » de
Robot. Il n'y a aucun traitement de gravité dans les rotations, copies ou fenêtres.

La convertir en charges explicites par élément est possible, mais cela change le format, les notes de calcul et les
moteurs. Ce n'est pas nécessaire pour retirer le concept, et je ne l'ai pas fait.

## 5. Conventions mathématiques vérifiées

- **Matrice** : R a pour colonnes x, y, z ; F_global = R·F_local, F_local = Rᵀ·F_global (`LoadResolver`).
- **Orthogonalité** : R est orthogonale, de déterminant +1, et y × z = x (test 565, toutes les barres de référence).
- **Forces et charges réparties** : vecteurs ordinaires, transformés par R.
- **Moments** : vecteurs axiaux. Une symétrie les inverse en plus de les réfléchir ; ce cas ne se présente plus, puisque
  les enregistrements sont recopiés comme dans Robot.
- **Rotation de section** : ce n'est pas un vecteur. γ se recalcule à partir de l'image du repère (§ 2), il ne se
  transforme ni comme une force ni comme un moment.
- **Moteurs** : les conventions du solveur sont inchangées. OpenSees reçoit `vecxz` = z local, comme avant ; seul le
  repère d'une barre verticale a changé, à γ égal.

## 6. Tests

| Test | Contrôle | Résultat |
|---|---|---|
| 558 | repère local de 19 barres de Robot (horizontales, inclinées, 3D, poteaux, sens inversés, γ de −45° à 45°) ; seuil sur 96 inclinaisons | conforme |
| 559 | copie par rotation (Z 30°, 90°, 180°, −45° ; X 90° ; Y 30°) : γ et repère de Robot, 48 barres | conforme |
| 560 | copie miroir (plans X, Y, Z, x = y) : γ et repère de Robot, 44 barres | conforme |
| 561 | rotation et symétrie sans copie ; inversion des nœuds | conforme |
| 562 | charges globales fixes, locales tournées, symétrie, copies multiples, moments, Annuler, enregistrement | conforme |
| 563 | migration : gravité, axes incohérents, surfaces, poteaux à γ = 0, 30°, −45°, 90° dans les deux sens, treillis, fichier de version 2 | conforme |
| 564 | calcul OpenSees comparé à Robot : 7 cas, 5 consoles IPE 300 | réactions à 1,3e-12 près ; déplacements à 2,3 % près |
| 565 | matrices orthogonales et directes, F_global = R·F_local, F_local = Rᵀ·F_global | conforme |

L'écart sur les déplacements n'est pas expliqué avec certitude. Les réactions, en équilibre, sont égales à 1e-12, ce qui
valide les directions et les charges. L'écart vient vraisemblablement de la déformation d'effort tranchant que Robot
inclut (aires de cisaillement AY, AZ dans sa section), alors que TSA utilise Euler-Bernoulli. Je ne l'ai pas vérifié.

Les tests 101, 141-142 et 553-554, ainsi que les tests de charges et de surfaces, ont été adaptés aux nouvelles
conventions. Leur intention est conservée. Les vérifications 210, 350, 374, 375 et 506 attendaient encore les anciens
signes ou le format 2 : corrigées à la reprise (§ 7). Test 101 : la copie miroir d'une poutre horizontale à γ = 15° a
γ = −15°, règle de Robot (§ 2).

## 7. Validation

Reprise du 2026-10-31 au soir, Release `build-ninja-release` (preset du dépôt, compilation complète : le dossier
`build-windows-validation` de TSA avait disparu du poste, cause inconnue).

- **Défaut trouvé à la reprise** : sans l'ancienne surcharge « nom », `MemberLoad::pointOnMember(…, axe, "P")` et
  `trapezoidal(…, axe, "nom")` convertissaient le nom en `isRelative = true` ; la charge sortait de la barre (test 539 :
  moment d'encastrement nul). Surcharges `const char*` et `std::string` rétablies dans `MemberLoad.h`.
- **Banc** : les charges surfaciques des cas JSON sans direction étaient lues suivant +Z (FEA-01 : flèche de signe
  opposé). `BenchmarkSuite` leur applique désormais `applyStoredSurfaceDirection`, comme aux charges sur barre.
- **TSA** : 60/60 suites, 572 tests, 0 échec (exécution sur bureau Windows séparé, aucune fenêtre). Compilation sans
  erreur ; aucun avertissement sur une ligne modifiée.
- **Banc officiel** (`TSA_Benchmark`, 39 cas) : 2 097/2 097.
- **TSALab** (branche `refactor/load-axes-section-rotation` créée) : `AnalyticalReference` (pression suivant Z global,
  q = −valeur) et `ExampleModels` (charges signées) adaptés ; 27/27.
- **Traductions** : 25 textes nouveaux en anglais et en malgache ; `check_translations.py --strict` sans erreur.
- **Couches** : `check_layers.py`, 0 violation. Dépendances Ninja des en-têtes présentes (BUG-011).
- **Non refait** : empreinte des 104 fichiers d'exemples et bancs au bit près contre `main` (scripts de comparaison absents
  du dépôt). Les exemples `.tsa` de format 2 sont migrés à la lecture (§ 4) ; l'identité des résultats n'est pas démontrée.

## 8. Limites et points non vérifiés

- **Coques et voiles** : le repère local d'une surface est celui de TSA (U vers le 2e sommet, N de Newell ; voile, V = +Z).
  Je ne l'ai pas comparé à celui des panneaux de Robot.
- **Nœuds** : une charge nodale reste globale. Un nœud n'a pas de repère local dans TSA ; Robot en propose par des
  appuis inclinés.
- **Semelles non carrées** : elles n'ont pas d'attribut d'orientation.
- **Vrai `TSA.exe`** : les fenêtres de charge modifiées n'ont pas été contrôlées à l'œil dans `TSA.exe`.
- **Autres environnements** : Linux et Debug ne sont pas vérifiés.
- **Robot** : ses fenêtres n'ont pas été utilisées ; seule l'API a été mesurée. Les opérations Édition > Rotation et
  Symétrie de l'interface utilisent vraisemblablement les mêmes fonctions (`SelRotate`, `SelMirror`), mais je ne l'ai pas
  vérifié.
- **Scripts d'essai** : `drive2.py` à `drive6.py`, dans le dossier temporaire de la session, sont résumés par le fichier
  de référence. Les relancer suppose Robot installé.
