# Rapport d'implémentation — outils de dessin BIM (17/10/2026)

Branche `feature/bim-drawing`, créée depuis `feature/io-ifc`. Rien n'est poussé ni fusionné.
Documents liés :
- constat : `BIM_DRAWING_AUDIT.md` ;
- architecture : `BIM_ARCHITECTURE.md` § 5 ;
- suite du chantier : `BIM_DRAWING_ROADMAP.md`.

## 1. Ce qui a changé

| Problème de l'audit | Correction | Fichiers |
| :--- | :--- | :--- |
| D1 Nœuds orphelins | Le tracé mémorise des points (`DrawPoint`). Les nœuds sont créés à la validation de l'élément ; Échap ne laisse rien. | `src/Interaction/ElementCreation.*`, `src/Viewer/OccView.cpp`, `OccView_Events.cpp` |
| D2 Annuler incomplet | Nœuds, élément et réglages du dialogue sont créés dans une seule `EditTransaction`, soit une entrée Annuler. | idem, dialogues Barre / Câble / Surface |
| D3 Logique dans la vue, 3 chemins | Un service sans vue, appelé par la vue et par les trois dialogues ; la vue ne fait plus qu'afficher le compte rendu. | idem |
| D4 Valeurs inventées | Préréglages Fondation et Treillis explicites et modifiables (2 onglets ajoutés). Il n'y a plus de poteau de 3 m au double clic : le clic est refusé avec son motif. Les matériaux absents de la bibliothèque sont retirés des listes (C20/25, C35/45, S275 devenaient C25/30 sans rien dire). | `src/Model/CreationPresets.h`, `src/UI/Dialogs/StructurePresetDialog.*`, `SurfaceCreationDialog.cpp` |
| D5 Pas de validation | Contrôles faits avant toute modification : longueur nulle, section sans aire, matériau sans module, doublon, dalle (3 points, alignés, non plane, croisée, points confondus, doublon), voile de dimension nulle, deuxième semelle sur un nœud. | `ElementCreation.cpp` |
| D6 Croisement non signalé | Avertissement si une extrémité tombe sur une barre sans nœud, ou si deux barres se croisent sans nœud commun. Aucune division automatique. | `ElementCreation.cpp` |
| D7 Tolérance locale | `GeometryTolerance::nodeCoincidence` (1 mm) devient la seule tolérance. Au-delà, un nouveau nœud est créé, sans fusion. | `src/Coordinate/GeometryTolerance.h` |
| D8 Quantités absentes | `QuantityTakeoff` calcule longueur, surfaces brute / ouvertures / nette, volume et masse, par élément, produit, matériau, section et famille. | `src/BIM/Core/QuantityTakeoff.*` |
| D9 Palette sans BIM | Panneau « Identité BIM et quantités » ajouté sous la vue de l'élément. | `src/UI/Properties/ElementBimPanel.*`, `PropertyPanel.*` |

Aucun module sans rapport avec le dessin n'a été modifié. Le format `.tsa` est inchangé : aucun
champ persistant n'a été ajouté, donc aucune migration n'est nécessaire.

## 2. État par exigence de la mission

| Exigence | État | Commentaire |
| :--- | :--- | :--- |
| Objet BIM = entité métier persistante | terminé | Produit physique, GlobalId et classe IFC pour tout élément dessiné (test 417). L'identité survit à Annuler / Rétablir, au `.tsa` et à l'IFC (test 423). |
| Séparation BIM / représentation / analytique / IFC | terminé | Le service écrit le modèle ; la vue n'affiche que le résultat ; le produit BIM se synchronise ; l'IFC reste dans `src/BIM/IFC`. |
| Cycle de création en 12 étapes, aucun objet invalide | terminé pour les modes Dessin et les 3 dialogues | Les outils `ModelingTool` de dessin restent transactionnels, mais sans les validations du service (partiel, feuille de route n° 1). |
| Accrochage centralisé | terminé | `SnapEngine` seul, tolérance de coïncidence unique. `GridSnapManager`, sans appelant, n'est pas encore retiré (non commencé, D13). |
| Croisement ≠ connexion | terminé | Tests 416, 423. |
| Pas de fusion par proximité | terminé | Test 410 (à 2 mm : nouveau nœud). |
| Aucun nœud, appui ou charge inventé | terminé | Aucun appui ni aucune charge n'est créé par les outils. Le poteau de 3 m implicite est supprimé (test 419). |
| Résultats périmés signalés | terminé | Test 418 : la création invalide les résultats, un refus non. |
| Quantités issues du modèle | terminé | Tests 420, 421, 423. Produits importés sans élément de calcul : pas de métré (limite documentée). |
| Palette de propriétés | terminé | Test 422. |
| Navigateur à hiérarchie BIM | non commencé | D10. |
| Types partagés (section, matériau) | non commencé | D11, demande une migration du format. |
| Grilles dans le modèle | non commencé | D12. |
| Persistance compatible / migrée | terminé | Format inchangé ; aller-retour `.tsa` vérifié (test 423). Les préréglages ne sont pas persistés, comme avant. |
| IFC | terminé | Export puis réimport des éléments dessinés : mêmes GlobalId (test 423). |
| Rien de bloqué | — | Aucun point bloqué. |

## 3. Tests

Nouvelle suite `drawing` (`tests/test_drawing.cpp`), enregistrée sous CTest (`TSA_drawingTests`) :

| Test | Objet |
| :--- | :--- |
| 410 | nœud : création, point confondu refusé, à 2 mm nouveau nœud, Annuler |
| 411 | poutre : nœuds et poutre dans une entrée Annuler, Rétablir, nœud accroché ou confondu réutilisé |
| 412 | refus avant modification : longueur nulle, section, matériau, doublons |
| 413 | poteau, treillis selon le préréglage, câble avec réglages du dialogue dans la même transaction |
| 414 | dalle : 2 points, alignés, non plane, croisée, confondus, côté replié, doublon ; création, Annuler |
| 415 | voile et semelle selon les préréglages, doublons |
| 416 | extrémité sur une barre et croisement : avertis, rien n'est divisé ni ajouté |
| 417 | produit IfcColumn avec GlobalId ; Annuler le retire, Rétablir rend le même GlobalId |
| 418 | résultats périmés après création, intacts après refus |
| 419 | **vraie vue OCCT pilotée par clics** : aucun nœud pendant le tracé, Échap ne laisse rien, Entrée valide une dalle, Annuler complet, double clic de poteau refusé |
| 420 | métré : poutre, dalle avec trémie, voile avec porte, semelle, treillis ; cumuls ; produit de 2 poutres |
| 421 | quantités recalculées après déplacement d'un nœud et après Annuler |
| 422 | palette : classe IFC, GlobalId, quantités, mise à jour, nœud sans panneau |
| 423 | scénario d'intégration : 4 poteaux, 4 poutres, dalle, voile, 4 semelles sur les nœuds existants ; aucun orphelin ; un produit par élément ; métré ; `.tsa` ; IFC ; 14 entrées Annuler |

Résultats mesurés sur ce poste (Release, MSVC 19.51, Qt 6.11.2) :

- suite `drawing`, plateforme native : **14 / 14** ;
- suite `drawing`, plateforme `offscreen` : 13 / 13, le test 419 étant **annoncé comme sauté**
  (BUG-076 : vue OCCT impossible sous `offscreen`) et non compté ;
- suite complète, plateforme native : voir § 5.

## 4. Ce qui n'a pas été vérifié

- **Essai de l'interface réelle de TSA** (UI Automation sur l'application complète) : non lancé
  dans ce chantier. Le test 419 pilote une vraie vue OCCT par événements souris, mais hors de la
  fenêtre principale (pas de ruban ni de dialogues réels). Aucun scénario de l'interface n'est
  déclaré validé visuellement.
- **CI GitHub** : la branche n'est pas poussée, aucun essai CI n'a donc été fait.

## 5. Suite complète et constats annexes

- Suite complète, plateforme native, avant le correctif de la touche Entrée (§ 6) : **418 / 418**.
- Version finale, plateforme native, deux exécutions :
  - **412 / 418** à la première. Le test 20 (mesure de temps, annulation à 5 000 barres sous
    350 ms) a dépassé son seuil et interrompu la suite `commands`. Ce test est déjà connu pour
    être instable sous charge (BUG-035).
  - La suite `commands` lancée seule trois fois : 7 / 7 à chaque fois (meilleur temps à
    5 000 barres ≈ 156 ms).
  - **418 / 418** à la seconde exécution complète.
- Deux lignes `libpng error: Read Error` apparaissent sur la sortie d'erreur pendant les tests des
  modèles de documents (vers le test 284). Elles figuraient déjà dans un journal CI antérieur à ce
  chantier et ne font échouer aucun test : rien à voir avec le dessin, non traité ici.

## 6. Défaut trouvé pendant la relecture

La touche Entrée du mode Dessin Dalle avait gardé une copie de l'ancien code : `addSlab` sur les
numéros de nœuds du tracé, sans validation. Avec des points libres (−1), elle aurait transmis des
identifiants invalides. Elle appelle désormais `finishCurrentSlab()` (même validation, même
transaction). Le test 419 couvre ce chemin.
