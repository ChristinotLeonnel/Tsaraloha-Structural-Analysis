# Rapport d'implémentation — outils de dessin BIM (17-18/10/2026)

Branche `feature/bim-drawing`, créée depuis `feature/io-ifc`. Elle n'est ni poussée ni fusionnée.
Documents liés :
- constat : `BIM_DRAWING_AUDIT.md` ;
- architecture : `BIM_ARCHITECTURE.md` § 5 ;
- suite du chantier : `BIM_DRAWING_ROADMAP.md` ;
- essai de l'interface : `tools/ui_tests/drawing_scenario.ps1`.

## 1. Ce qui a changé

### 1.1 Problèmes de l'audit (17/10)

| Problème | Correction | Fichiers |
| :--- | :--- | :--- |
| D1 Nœuds orphelins | Le tracé mémorise des points (`DrawPoint`) ; les nœuds sont créés à la validation de l'élément ; Échap ne laisse rien. | `src/Interaction/ElementCreation.*`, `src/Viewer/OccView.cpp`, `OccView_Events.cpp` |
| D2 Annuler incomplet | Nœuds, élément et réglages du dialogue dans une seule `EditTransaction`, donc une entrée Annuler. | idem, dialogues Barre / Câble / Surface |
| D3 Logique dans la vue, 3 chemins | Un service sans vue, appelé par la vue, les trois dialogues et les outils de dessin avancés. | idem, `src/Interaction/Tools/DrawTools.cpp` |
| D4 Valeurs inventées | Préréglages Fondation et Treillis explicites et modifiables. Plus de poteau de 3 m au double clic, plus de barre de treillis de 0,10 m imposée. Matériaux absents de la bibliothèque retirés des listes (C20/25, C35/45, S275 devenaient C25/30 sans rien dire). | `src/Model/CreationPresets.h`, `src/UI/Dialogs/StructurePresetDialog.*`, `SurfaceCreationDialog.cpp` |
| D5 Pas de validation | Contrôles avant toute modification : longueur nulle, section sans aire, matériau sans module, doublons, dalle (3 points, alignés, non plane, croisée, points confondus), voile de dimension nulle, deuxième semelle sur un nœud. | `ElementCreation.cpp` |
| D6 Croisement non signalé | Avertissement si une extrémité tombe sur une barre sans nœud, ou si deux barres se croisent sans nœud commun. Aucune division automatique. | `ElementCreation.cpp` |
| D7 Tolérance locale | `GeometryTolerance::nodeCoincidence` (1 mm) seule tolérance ; au-delà, nouveau nœud, sans fusion. | `src/Coordinate/GeometryTolerance.h` |
| D8 Quantités absentes | `QuantityTakeoff` : longueur, surfaces brute / ouvertures / nette, volume, masse, par élément, produit, matériau, section et famille. | `src/BIM/Core/QuantityTakeoff.*` |
| D9 Palette sans BIM | Bloc « Identité BIM et quantités » sous la vue de l'élément (produit, classe IFC, GlobalId, étage, type, métré). | `src/UI/Properties/ElementBimPanel.*`, `PropertyPanel.*` |
| D10 Navigateur sans hiérarchie BIM | Branche « Structure BIM (IFC) » : site › bâtiment › étages › produits, et types de produits. | `src/UI/ModelTree/ModelTreeWidget.*`, `src/UI/Common/SelectionSynchronizer.cpp` |
| D11 Types copiés par valeur | Types de produits partagés (`ProductType`) : création depuis le modèle, propagation, réapplication, écarts, IFC. | `src/BIM/Core/BimModel.*`, `src/BIM/Core/ProductTypes.*`, `src/BIM/IFC/*` |

### 1.2 Feuille de route validée le 18/10

| Étape | Résultat | Commit |
| :--- | :--- | :--- |
| 1. Outils avancés par le service | Chaîne, rectangle, portique, contreventement en X, arc, poteaux sur grille passent par `ElementCreation` ; l'outil entier est annulé au premier refus. Un rectangle posé sur des poutres existantes les **doublait en silence** : il est désormais refusé (test 148 adapté). | `c329057` |
| 2. Validation dans l'interface complète | Scénario UI Automation (`drawing_scenario.ps1`). Il a trouvé et fait corriger trois défauts (§ 4). | `49e3e42` |
| 3. Arbre par bâtiment et étages | Branche « Structure BIM » ; un clic sur un produit ouvre son élément de calcul ou l'inspecteur IFC. | `746c473` |
| 4. Voiles à base inclinée | Calcul, métré et IFC vérifiés **ensemble** (§ 5). | `c4d9ec8` |
| 5. Types partagés | Types de sections et de matériaux, persistance compatible, IFC (§ 6). | ce commit |

## 2. État par exigence de la mission

| Exigence | État | Commentaire |
| :--- | :--- | :--- |
| Objet BIM = entité métier persistante | terminé | Produit, GlobalId, classe IFC, type, pour tout élément dessiné ; identité conservée par Annuler / Rétablir, `.tsa` et IFC (tests 417, 423, 430). |
| Séparation BIM / représentation / analytique / IFC | terminé | Le service écrit le modèle, la vue affiche, le BIM se synchronise, l'IFC reste dans `src/BIM/IFC`. |
| Cycle de création, aucun objet invalide | terminé | Modes Dessin, trois dialogues, six outils avancés (tests 410-416, 425). |
| Accrochage centralisé | terminé | `SnapEngine` seul, tolérance unique. `GridSnapManager` (sans appelant) n'est pas retiré (D13, non commencé). |
| Croisement ≠ connexion | terminé | Tests 416, 423, 425. |
| Pas de fusion par proximité | terminé | Test 410. |
| Aucun nœud, appui ou charge inventé | terminé | Tests 419, 425. |
| Résultats périmés signalés | terminé | Tests 418, 431. |
| Quantités issues du modèle | terminé | Tests 420, 421, 423, 428. Produit importé sans élément de calcul : pas de métré (limite documentée). |
| Palette de propriétés | terminé | Tests 422, 426 ; essai de l'interface. |
| Navigateur à hiérarchie BIM | terminé | Test 427 ; essai de l'interface. |
| Types partagés | terminé (cœur) | Tests 430-432 ; essai de l'interface. Pas d'éditeur de type dédié (§ 7). |
| Voiles à base inclinée | terminé | Tests 424, 428, 429 ; essai de l'interface. Porte au pied refusée sur une base inclinée. |
| Persistance compatible | terminé | Format `.tsa` inchangé (1.11) ; types en champs facultatifs du bloc BIM, schéma 1 inchangé ; ancien fichier lu tel quel (test 430). |
| IFC | terminé | Éléments dessinés, voiles inclinés et types : export, relecture par IfcOpenShell, réimport (tests 423, 428, 432). |
| Grilles dans le modèle | non commencé | D12. |
| Rien de bloqué | — | Aucun point bloqué. |

## 3. Tests

Suite `drawing` (`tests/test_drawing.cpp`, CTest `TSA_drawingTests`) et un test de la suite `surfaces` :

| Test | Objet |
| :--- | :--- |
| 410-416 | service de création : nœud, poutre transactionnelle, refus, poteau / treillis / câble, dalle, voile / semelle, croisements |
| 417-418 | identité BIM des éléments dessinés ; résultats périmés après création, intacts après refus |
| 419 | **vraie vue OCCT pilotée par clics** : aucun nœud pendant le tracé, Échap, Entrée, Annuler, double clic de poteau refusé (sauté et annoncé sous `offscreen`, BUG-076) |
| 420-421 | métré (trémie, porte, cumuls, produit de 2 poutres) ; quantités recalculées |
| 422, 426 | bloc BIM seul, puis dans la palette complète (régression trouvée par l'essai de l'interface) |
| 423 | scénario d'intégration : 14 créations, aucun orphelin, un produit par élément, métré, `.tsa`, IFC, 14 entrées Annuler |
| 424 | voile à base inclinée : accepté et signalé au-delà de 1 mm, cotes conservées, Annuler |
| 425 | outils avancés : portique refusé sans trace (y compris dans une transaction englobante), une entrée Annuler, préréglage Treillis, croisement signalé, arc en double refusé |
| 427 | arbre « Structure BIM » : étages, produits, signal au clic, mise à jour après Annuler |
| 428 | voile incliné : repère orthonormé, aire, ouvertures, métré, aller-retour IFC, **volume relu par IfcOpenShell = 2,4 m³** |
| 429 (surfaces) | voile incliné **calculé en coques** : poids propre = Σ réactions = ρ·g·t·L·H = 58,86 kN, éléments dans le parallélogramme |
| 430-432 | types : création (migration), `.tsa`, ancien bloc sans types ; écart, propagation (une entrée Annuler), réapplication ; IFC (fichier validé par IfcOpenShell, réimport) |
| 148 (tools) | adapté : le rectangle superposé à une chaîne existante est refusé (il doublait deux poutres) |

Résultats mesurés sur ce poste (Release, MSVC 19.51, Qt 6.11.2, plateforme native) :

- suite `drawing` : **22 / 22** ; suite `surfaces` : 12 / 12 ; suite `tools` : 10 / 10 ;
- suite complète : **427 / 427** (18/10/2026, version finale).

Le test 20 (mesure de temps d'annulation, BUG-035) a échoué deux fois sur une dizaine
d'exécutions complètes, toujours sous charge. Les meilleurs temps mesurés à 5 000 barres vont de
22 ms à 508 ms selon la charge du poste. Les suites qui le précèdent n'exécutent aucun code de ce
chantier, et le seuil n'a pas été modifié.

## 4. Essai de l'interface réelle

`tools/ui_tests/drawing_scenario.ps1` pilote TSA par UI Automation : vrais clics dans la vue 3D,
vrais dialogues, ruban et menus contextuels. Dernière exécution : **17 / 17 vérifications
observées**, captures conservées dans le dossier d'essai.

| Étape | Observé |
| :--- | :--- |
| Poutre | 1er clic sans nœud ; Échap sans trace ; poutre au 2nd clic ; Annuler retire poutre et nœuds ; Rétablir |
| Dalle | 3 clics sans nœud ; Entrée : dalle et ses 3 nœuds ; dalle croisée refusée (« Le contour se recoupe ») |
| Voile incliné | créé, invite « Attention : Base inclinée : les deux pieds du voile diffèrent de 0.400 m » |
| Préréglages | onglets Fondation et Treillis |
| Portique | créé ; relancé sur les mêmes pieds : « Portique non créé : … élément en double » (console), rien de créé |
| Palette | bloc « Identité BIM et quantités » de la dalle (classe IfcSlab, GlobalId, surface, volume) |
| Arbre | Structure BIM › Site › Bâtiment › RDC › Poteau C1 ; clic → propriétés du poteau |
| Types | clic droit sur la branche BIM → « Créer les types… » : 5 types, branche « Types de produits » |

Défauts de TSA trouvés par cet essai et corrigés :

1. Annuler (bouton et Ctrl+Z) restait **grisé après un tracé dans la vue 3D**. Le signal
   `elementCreated` n'avait aucun abonné ; ce défaut est antérieur au chantier.
2. Le bloc BIM ne s'affichait jamais dans l'application : il ne recevait pas le modèle. Le test
   unitaire du bloc seul ne pouvait pas le voir (test 426 ajouté).
3. Après Échap en mode Poutre ou Poteau, le dialogue « Barre » gardait l'origine du tracé annulé.

Un premier essai, dont le script n'avait pas de garde-fou, a ouvert un projet récent de
l'utilisateur. Le fichier n'a pas été modifié sur disque (vérifié), et l'essai s'arrête désormais
sans rien enregistrer si la fenêtre active n'est pas le projet qu'il a créé.

## 5. Voiles à base inclinée

Un voile est extrudé verticalement depuis sa base. Si ses pieds diffèrent de cote, son élévation
est un parallélogramme d'aire longueur en plan × hauteur. Toutes les représentations utilisent
désormais cette géométrie :

| Représentation | Avant | Après |
| :--- | :--- | :--- |
| Affichage 3D | parallélogramme | inchangé |
| Calcul (`wallSurface`) | repère non orthogonal, aire L₃D × H | repère U horizontal / V vertical, aire L × H |
| `Wall::length()` (poids propre total, ouvertures) | longueur 3D | longueur en plan |
| Métré | longueur 3D | longueur en plan |
| Export IFC | bloc horizontal à la cote du 1er pied | parallélogramme extrudé dans l'épaisseur |
| Réimport IFC | « contour non rectangulaire non pris en charge » | voile recréé |

À la création, un voile incliné est accepté avec l'avertissement « Base inclinée », car il
s'agit souvent d'une erreur de niveau visé. Limite restante : une porte au pied d'un voile à base
inclinée est refusée, avec son motif.

## 6. Types de produits partagés

- **Modèle** : `ProductType` dans le `BimModel`, rattaché aux produits par `typeId`, avec
  section et matériau (éléments linéaires), épaisseur et matériau (dalles, voiles), matériau
  (fondations). Sauvegardé avec le modèle (Annuler / Rétablir).
- **Persistance** : champs facultatifs du bloc BIM (`types`, `type`), schéma inchangé. Un
  fichier existant se lit tel quel, avec des produits sans type ; une version antérieure de TSA
  ignore les types sans rejeter le bloc BIM.
- **Opérations**, chacune en une transaction (menu contextuel de l'arbre) :
  - créer les types depuis le modèle (migration) ;
  - propager la définition d'un élément à son type ;
  - réappliquer un type à ses occurrences.
  L'écart d'une occurrence est affiché dans la palette et l'arbre.
- **IFC** : `Ifc*Type` et `IfcRelDefinesByType` à l'export ; à l'import, types recréés avec
  leur GlobalId, définition reprise de la première occurrence.

## 7. Limites et reste à faire

- Types : pas d'éditeur de type dédié. On modifie une occurrence puis on propage. Les câbles
  n'ont pas de type, et le matériau n'est associé qu'aux occurrences en IFC, pas au type.
- Porte au pied d'un voile à base inclinée.
- Grilles dans le `Model` (D12), unification des accrochages (D13, constat corrigé : les réglages
  de `GridSnapManager` sont encore lus par la vue), préréglages persistés, libellés de matériaux
  par la bibliothèque : non commencés, planifiés dans `BIM_DRAWING_ROADMAP.md` (chantiers A à E).
- CI (BUG-076) : correctif préparé et vérifié en local (voir § 8), pas encore exécuté sur le runner.
- Indicateur « SNAP: ACTIF » de la barre d'état jamais mis à jour (BUG-078, existant avant ce
  chantier) : corrigé le 18/10/2026 avec la finalisation des vues (docs/VIEWS_NAVIGATION.md).
- Messages « No operation defined for IfcVertexPoint » du noyau d'IfcOpenShell à la relecture
  d'un export TSA : points de connexion analytiques sans géométrie physique ; sans effet sur
  l'import.

## 8. Vérification avant publication (18/10/2026)

Dépôt : `feature/bim-drawing` = `origin/feature/io-ifc` (94c841e) + 7 commits locaux, sans le
correctif Eigen `0157c89` (qui ne touche que `scripts/build_ifcopenshell.ps1`, fichier absent des
48 fichiers de la branche ; fusion d'essai `git merge-tree` sans conflit). Aucune branche distante
`feature/bim-drawing`.

Exécutions réelles de ce jour, compilation `build-ninja-release` à jour avec `79dfdea` :

| Vérification | Résultat |
| :--- | :--- |
| Passe CI `offscreen` (sans les suites à vue 3D) | 25 / 25 suites CTest |
| Passe CI `windows` + Mesa llvmpipe (AllTests, audit, display, multidoc, drawing, ifc) | 6 / 6, suite complète 427 / 427, aucun test sauté ; rendu « Mesa — llvmpipe, OpenGL 4.6 » |
| Scénario d'interface `drawing_scenario.ps1` (run26) | 17 / 17, capture finale examinée |
| Anciens `.tsa` du dépôt (9 exemples du 11-12/10, 2 fichiers d'essai du 07/10) ouverts par `tsa-ifc export` | 11 / 11 ouverts et exportés (contrôle IfcOpenShell), fichiers inchangés |

La suite `demo` reconstruit ses projets avant de les relire : elle ne prouve pas à elle seule la
lecture des anciens fichiers, d'où l'ouverture des fichiers du dépôt ci-dessus.

Couverture par l'interface réelle et par les seuls tests unitaires :

| Exigence | Interface réelle (17 vérifications) | Tests unitaires seulement |
| :--- | :--- | :--- |
| Six outils avancés | portique (créé, puis refusé en double) | chaîne, rectangle, X, arc, grille (425) |
| Refus des géométries invalides et des doublons | dalle croisée, portique en double | alignés, non plan, confondus, doublons de barre, de dalle, de voile, de fondation (412, 414, 415) |
| Échap, Annuler, Rétablir | poutre : Échap, Annuler, Rétablir | dalle, outils, types (411, 419, 425, 431) |
| Arbre Structure BIM | chemin jusqu'au poteau, clic vers les propriétés | classement complet (427) |
| Panneau d'identité et de quantités | dalle : classe IFC, GlobalId, quantités | modification, type et écart (422, 426, 431) |
| Types partagés | création par le menu de l'arbre | propagation, réapplication, Annuler, IFC (430-432) |
| Voiles inclinés | création et avertissement | calcul (429), métré et IFC (428) |
| Anciens `.tsa` | — | bloc BIM sans types (430) ; fichiers réels ouverts ci-dessus |

Le test 20 reste instable sous charge (BUG-079), seuil inchangé.

