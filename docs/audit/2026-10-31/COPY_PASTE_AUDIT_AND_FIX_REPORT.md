# Audit et correction du Copier / Coller de TSA

Date : 2026-10-31. Branche : `fix/copy-paste-complete`, créée depuis `main` (`52274ee`). Aucun commit, fusion ni push.

## 1. Résultat

Copier un objet reproduit maintenant toutes ses données copiables, quelle que soit la commande utilisée. Votre exemple est
corrigé : une poutre portant une charge ponctuelle, copiée avec Ctrl+C puis collée avec Ctrl+V, garde sa charge. Les outils de la
vue 3D (Copier, réseaux, rotation, décalage, symétrie) la gardent aussi, alors qu'ils n'avaient pas de copie de charges du tout.

Validation : 59 suites sur 59, 564 tests réussis, aucun résultat de calcul modifié (§ 7).

## 2. Diagnostic initial

### Cartographie

| Interface | Point d'entrée | Moteur avant correction |
|---|---|---|
| Ctrl+C, Ctrl+X, Ctrl+V, Ctrl+Maj+V, menu Édition, ruban | `MainWindow::onActionCopyClipboard`, `pasteClipboardAt` | `StructuralClipboard` |
| Copier 3D, Copie numérique (Ctrl+D), Réseau linéaire | `CopyTool`, `LinearArrayTool` → `Model::copyElements` | `copyTransformed` |
| Rotation avec copie, Réseau polaire | `RotateTool`, `PolarArrayTool` → `Model::copyAndRotateElements` | `copyTransformed` |
| Décaler | `OffsetTool` → `Model::copyElements` | `copyTransformed` |
| Symétrie avec copie | `MirrorTool` → `Model::mirrorElements` | code propre à la symétrie |
| Fenêtre des charges surfaciques, Dupliquer | `SurfaceLoadsDialog` | copie par valeur de la ligne : correcte |
| Grilles, Dupliquer | `GridManager::duplicateGrid`, `DuplicateGridCommand` | autre domaine : correct |
| Blueprint, graphes | `BlueprintEditor::copySelection` | autre domaine : correct |
| Diagrammes 2D, Exporter | `Diagram2DWidget` | image ou tableau, presse-papier Windows |

Il n'existe ni menu contextuel de copie dans l'arborescence ou la vue, ni moteur propre à la vue 2D : elle partage `OccView` et
les mêmes commandes.

### Causes des pertes de données

| # | Cause | Fichier | Effet |
|---|---|---|---|
| 1 | Option `loads` à faux par défaut, réglage `Clipboard/copyLoads` à faux | `StructuralClipboard.h`, `MainWindow_Actions.cpp` | Ctrl+C sans charges : votre anomalie |
| 2 | Deux moteurs de copie, et un troisième dans la symétrie | `Model_Transformations.cpp`, `Model_Topology.cpp` | comportements différents pour une même opération |
| 3 | `copyTransformed` ignorait le `LoadManager` | `Model_Transformations.cpp` | aucune charge copiée par les outils |
| 4 | `ElementSet` transmis sans voiles ni fondations | `ModifyTools.cpp` | voiles et fondations ignorés sans message |
| 5 | `copySlabAttributes` limité à quatre attributs | `ModelElementCopy.h` | ouvertures, appuis de bord et formulation perdus |
| 6 | Copie miroir sans appuis ni treillis, dalle sans ouvertures | `Model_Topology.cpp` | nœuds copiés sans appui |
| 7 | Couleur du poteau, nom et couleur du nœud omis au collage | `StructuralClipboard.cpp` | présentation perdue |
| 8 | Aucune notification après collage d'une charge surfacique | `StructuralClipboard.cpp` | observateurs non prévenus |
| 9 | Cas de charge rapproché par nom seul | `StructuralClipboard.cpp` | deux cas homonymes : charge sur le mauvais cas |
| 10 | Ctrl+C sur une charge seule | `MainWindow_Transform.cpp` | presse-papier vidé sans explication |

### Données copiées

| Donnée | Presse-papier avant | Outils avant | Maintenant, partout |
|---|---|---|---|
| Géométrie, connectivité, section, matériau, rotation, relâchements | oui | oui | oui |
| Charges nodales, sur barre, surfaciques | option désactivée | **non** | oui, option activée par défaut |
| Appuis de nœud | oui | oui, **non** en miroir | oui |
| Voiles, fondations | oui | **non** | oui |
| Treillis | oui | oui, **non** en miroir | oui |
| Dalle : ouvertures, appuis de bord, formulation | oui | **non** | oui |
| Métadonnées BIM | oui | oui, **non** en miroir | oui |
| Nom et couleur du nœud, couleur du poteau | **non** | **non** | oui |
| Noms d'éléments | oui | **non** | oui |
| Cotations | oui | non | presse-papier seulement |

## 3. Stratégie retenue

Aucune architecture nouvelle : `StructuralClipboard`, déjà le plus complet, devient le moteur unique. Les remappages de nœuds,
d'éléments et de cas de charge n'existent plus qu'à un seul endroit.

- `StructuralClipboard::pasteImage` colle l'image de la sélection par une transformation rigide.
- `Model::copyElements`, `copyAndRotateElements` et `mirrorCopyElements` prennent un instantané par valeur, puis collent chaque
  image. L'ancien `copyTransformed` est supprimé, ainsi que la branche de copie de `mirrorElements`.
- `CopyTransform.{h,cpp}` regroupe les calculs d'image : des fonctions pures, testables sans modèle.
- Les outils lisent les mêmes options que le presse-papier (`Clipboard/copySupports`, `Clipboard/copyLoads`).
- Les transactions existantes sont conservées : `pasteUndoable` pour Ctrl+V, `EditTransaction` de
  `applyActiveModelingTool` pour les outils. Une copie est une seule étape d'annulation, et les résultats sont périmés par la
  révision du modèle.

### Charges et transformations

Une translation laisse tout identique. Une rotation ou une symétrie doit transformer les charges, sinon le calcul serait faux
sans alerte. La charge copiée est l'image de la charge d'origine.

| Donnée | Rotation | Symétrie |
|---|---|---|
| Force | tournée | image |
| Moment | tourné | image de sens opposé (vecteur axial) |
| Charge globale non alignée sur un axe | une charge par composante | idem |
| Charge locale | exprimée dans le repère local de la copie | idem |
| Gravité | reste la gravité | reste la gravité |
| Dalle : (u, v), zone, gradient, ouvertures | inchangés | convertis, contour inversé |
| Dalle : bords d'appui | inchangés | indices convertis |
| Voile : normale, excentrement | inchangés | opposés à leur image |

Dépendances globales : les cas de charge et combinaisons ne sont jamais dupliqués dans un même projet. Le cas d'origine est
retrouvé par son identifiant, puis, dans un autre projet, par son nom et sa catégorie.

Un nœud situé sur le plan d'une copie miroir reste partagé. Sa charge nodale n'est pas recopiée, ce qui la doublerait.

## 4. Fichiers modifiés

| Fichier | Modification |
|---|---|
| `src/Model/CopyTransform.{h,cpp}` | **nouveaux** : image des charges et des données de surface |
| `src/Model/StructuralClipboard.{h,cpp}` | moteur unique, `pasteImage`, `pasteLoads`, option de charges activée, attributs de nœud et de poteau, cas homonymes, notification |
| `src/Model/Model.h`, `Model_Transformations.cpp`, `Model_Topology.cpp` | `copyTransformed` remplacé, `mirrorCopyElements` ajouté |
| `src/Interaction/Tools/ModifyTools.cpp` | sélection complète, options communes, avertissements affichés |
| `src/UI/MainWindow_Actions.cpp`, `MainWindow_Transform.cpp` | valeur par défaut, charge seule signalée sans vider le presse-papier |
| `cmake/TSAProduct.cmake`, `tools/ci/run_test_suites.py` | nouveaux fichiers, suite `copypaste` |
| `tests/test_copy_paste.cpp` (**nouveau**), `test_common.h`, `test_main.cpp` | tests 549 à 557 |
| `translations/tsa_en.ts`, `tsa_mg.ts` | nouveaux messages |
| `docs/MULTI_DOCUMENT.md`, `DOCUMENTATION.md`, `docs/ANALYTICAL_MODEL.md`, `.claude/*` | comportement documenté |

## 5. Couverture des scénarios demandés

| Scénario | Test |
|---|---|
| 1 à 5 : poutres sans charge, ponctuelle, réparties, mélange, plusieurs éléments | 549 |
| 6, 7, 8 : matériau et section, appuis, groupe connecté | 550 |
| 9, 10, 11 : copies répétées, Annuler, Rétablir | 551 |
| 12, 13 : vues 3D et 2D | 556 |
| 14 : interface numérique (paramètres de l'outil, comme la fenêtre de saisie) | 552 |
| 15 : enregistrer puis recharger | 555 |
| Identifiants uniques, références, valeurs exactes, unités, repères | 549, 553, 554, `referencesValid` dans chaque test |
| Cas de charge non dupliqués | 549, 557 |
| Rotation, symétrie, voiles, fondations, autre projet, options | 552, 553, 554, 557 |

Les tests lisent les données du modèle. Pour les rotations et symétries, ils comparent la charge copiée à l'image de l'original
par `LoadResolver::memberLoadVector`, la fonction de résolution du calcul.

## 6. Limites et points non couverts

**Décisions à prendre**

- **Rotation de section** : la valeur β est conservée telle quelle, y compris en symétrie, où l'image exacte d'une section
  tournée serait −β. Le test 101 fige ce comportement. Pour un poteau copié par rotation autour de Z, le repère local suit le
  repère global : la section ne tourne donc pas avec la copie. Une section non symétrique ou tournée peut ainsi différer de son
  image exacte. Je ne l'ai pas changé, car cela modifierait un comportement validé par un test.
- **Gravité** : elle reste verticale lors d'une rotation ou symétrie qui incline la structure. Un avertissement le signale. Une
  charge nodale en Fz négative, elle, suit la structure : la distinction est volontaire, mais à connaître.

**Non copié, par conception**

- Résultats de calcul : jamais copiés. Toute copie change la révision du modèle.
- Charge seule : elle se copie avec son élément. Ctrl+C sur une charge seule affiche un message.
- Paramètres de maillage et zones de raffinement : données globales du projet, en coordonnées, pas des données d'élément.
- Cotations par les outils de copie : presse-papier seulement.

**Limites de géométrie**

- Voile : il monte selon +Z. Une rotation ou symétrie qui l'inclinerait ne le copie pas, avec un avertissement.
- Semelle non carrée : pas d'attribut d'orientation, elle reste alignée sur les axes globaux, avec un avertissement.

**Hors périmètre, défaut repéré**

- La symétrie **sans** copie (retournement) inverse le contour d'une dalle sans convertir ses ouvertures ni ses charges en (u, v). Ce
  n'est pas une copie, je ne l'ai pas modifiée. Ce défaut mérite une entrée de suivi : voir BUG-171.

**Non vérifié**

- Vrai `TSA.exe` : les tests pilotent les outils, `OccView` et le presse-papier réels, mais pas `MainWindow`. Les raccourcis
  Ctrl+C et Ctrl+V eux-mêmes, le message sur une charge seule et l'affichage des avertissements ne sont pas exercés.
- Fenêtre `SurfaceLoadsDialog` : relue, non testée, et non modifiée.
- Linux et Debug.
- Fuites mémoire : non mesurées faute d'outil sur ce poste. Le code ajouté n'utilise que des valeurs et des conteneurs standard,
  sans allocation manuelle.

**À savoir**

- Le réglage `Clipboard/copyLoads` n'est enregistré que si l'utilisateur coche ou décoche l'option. Si vous l'avez déjà
  décochée une fois, elle reste décochée : Édition > Options du presse-papier > Copier les charges.
- Les outils ne reprennent plus un nom automatique pour une copie : le nom de l'original est conservé, comme avec Ctrl+V.

## 7. Tests exécutés (Windows, Release)

| Contrôle | Résultat |
|---|---|
| `TSA_TestSuite`, toutes suites | 59/59, 564 réussis, 0 échec, 1 ignoré |
| Suite `copypaste`, tests 549 à 557 | 9/9 |
| Tests existants | tous réussis, aucun modifié |
| TSALab, compilé puis testé après TSA | 27/27 |
| Empreinte des exemples (scripts Tcl et résultats) | 104 fichiers identiques à l'octet à la référence d'avant ce travail |
| Banc officiel | 2 097/2 097, aucun écart de valeur |
| `check_layers.py`, `check_shortcuts.py --strict`, `check_translations.py --strict` | conformes |

Défauts trouvés pendant la validation : aucun sur le code livré. Tous les échecs intermédiaires venaient des tests eux-mêmes (noms de
champs, modèle neuf contenant déjà un cas Q).
