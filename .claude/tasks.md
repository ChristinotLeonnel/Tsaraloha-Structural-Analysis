# Tasks

Last Updated: 2026-10-04. Uniquement des tâches réellement identifiées (voir known-issues.md).

## CRITICAL

- [ ] (aucune tâche critique ouverte)

## HIGH

- [ ] BUG-002 — Mailler dalles/voiles pour le calcul (l'avertissement explicite avant calcul est fait le 2026-10-04).
- [ ] BUG-011 — Installer le pack de langue anglais de Visual Studio (action utilisateur) puis reconfigurer ; vérifier `ninja -t deps`.

## MEDIUM

- [ ] BUG-001 — Calcul OpenSees non bloquant (évaluer `OpenSeesSolver::solveAsync` existant) avec progression et annulation.
- [ ] BUG-003 — Inclure niveaux / grilles / WorkPlanes dans l'historique Undo (sans redéclencher `levelElevationChanged` à la restauration).
- [ ] BUG-004 / BUG-009 — Brancher ou retirer les commandes d'isolation `cmd.isolate.*` et le module `src/View3D/Isolation`.
- [ ] BUG-005 — Édition multi-objets dans le panneau Propriétés (via `EditTransaction`).
- [ ] Vérifications manuelles GUI non automatisées : enchaînement Z→X→Y, entrée/sortie mode 2D, sélection d'un câble fin, Undo après édition de propriété, message « résultats obsolètes ».
- [ ] Édition géométrique manquante : division à un nœud existant / aux intersections, symétrie par plan quelconque, ouvertures dalles/voiles, grips viewport.
- [ ] Vérification manuelle GUI : Symétrie, Diviser les barres, Fusionner les nœuds (menu Édition / ruban), Undo de chacune.

- [ ] BUG-016 — Diagrammes intermédiaires exacts (sectionForce ou reconstruction M(x)/V(x) depuis basicForce + charges locales).
- [ ] Vérification manuelle GUI : dock « Données d'analyse », configuration LIGHT/ADVANCED, export, charge sur poteau via MemberLoadDialog.

## IA Co-Engineering (ADR-015)

- [ ] What-If : `SimulationScenario` (copie du modèle via snapshot, calcul, comparaison, jamais d'écrasement).
- [ ] Rapport d'ingénierie IA (15 sections, citations des données TSA, réutiliser le moteur NDC).
- [ ] « Expliquer avec l'IA » depuis les docks Propriétés et Résultats (actuellement : arbre + ruban).
- [ ] Distribution : embarquer une build llama.cpp testée dans `<app>/ai/llama` (détectée en priorité) ;
      aujourd'hui : llama.cpp installé via winget.
- [ ] Évaluer la qualité des réponses avec Qwen3 4B/8B sur une machine non saturée (BUG-021).
- [ ] BUG-020 — charger les traductions Qt (qtbase_fr).

## Projets récents / aperçus

- [ ] Restaurer aussi l'état d'affichage (viewState) via les actions de l'interface (grille, calques, 2D).
- [ ] Vérifier visuellement la barre d'état après correction du chevauchement (1920 px et 1536 px).
- [ ] Option : masquer cube de navigation / trièdre pendant la capture d'aperçu.

## LOW

- [ ] BUG-017 — Contrôle d'équilibre en moments.
- [ ] BUG-018 — Famille dans les liens `tsa://element` de la NDC.
- [ ] BUG-019 — Tester un nœud relié uniquement à des treillis en -ndf 6.
- [ ] Persistance optionnelle des résultats avancés (lié à BUG-013).

- [ ] BUG-006 — Ajouter des `EditRecord` aux appels directs de `pushUndoState` (dialogues, viewport).
- [ ] BUG-007 — Index id → item dans `ModelTreeWidget`.
- [ ] BUG-008 — Mesurer l'ouverture en Release ; partager la géométrie des sphères de nœuds si nécessaire.
- [ ] BUG-010 — Remplacer `TColgp_HArray1OfPnt` par `NCollection_HArray1<gp_Pnt>`.
- [ ] BUG-013 — Persister paramètres d'analyse / résultats (chunks SETT / RSLT) si souhaité.
- [ ] Fichiers > 2 000 lignes (OccView_Navigation.cpp, OccView_Shapes.cpp, MainWindow_Actions.cpp) : analyse de découpage avant d'y ajouter des responsabilités.

## COMPLETED

- [x] Extraction avancée OpenSees : mapping TSA↔OpenSees, U/R/forces local-global-basic, matrices élémentaires et K_global, export, contexte IA, dock ; corrections FIX-015 à FIX-022 (2026-10-04, tests 105–111, non commité).

- [x] Symétrie, division de barres, fusion de nœuds confondus + avertissement dalles/voiles non calculés (2026-10-04, tests 101–104, non commité).

- [x] Audit stabilité/performance + corrections (merge 5ed3cf9) — FIX-001 à FIX-008, FIX-014.
- [x] Système d'édition : invalidation des résultats, transactions, historique structuré, sélection ensembliste, règle des niveaux, budget mémoire (merge 81e34cf) — FIX-009 à FIX-013.
- [x] Initialisation de la mémoire technique `.claude/` (2026-10-03).
