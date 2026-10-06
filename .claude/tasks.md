# Tasks

Last Updated: 2026-10-06. Uniquement des tâches réellement identifiées (voir known-issues.md).

## BIM (ADR-020, docs/BIM_ARCHITECTURE.md)

- [x] Phases 1–3 : noyau BIM, identifiants stables, mapping physique → analytique, chunk BIMM (tests 170–176).
- [x] Export IFC 4.3 (STEP) + validation IfcOpenShell ; import IFC ; test aller-retour (tests 177–180).
- [ ] BUG-028 : charges / combinaisons IFC (IfcStructuralLoadGroup…), relâchements, unités dérivées, grilles.
- [ ] Repère de grille « B-3 » des produits (information BIM, GridManager hors Model).
- [ ] Validation BIM pré-calcul, sous-ensemble IDS, abstraction BCF, API IA contrôlée.
- [x] UI : actions Exporter / Importer IFC (essai GUI 2026-10-06).
- [ ] UI : propriétés BIM (GlobalId, catégorie, Psets) dans le panneau Propriétés.
- [ ] BUG-029 : transmettre les métadonnées BIM au collage (`StructuralClipboard`).
- [ ] Docs restantes : BIM_GUIDELINES.md, AI_API.md, VERSIONING.md (IFC_MAPPING.md fait).

## Fenêtre / Start Center (ADR-021)

- [x] AppShell, barre de titre, Start Center, Nouveau projet, Fermer le projet (essais GUI 2026-10-06).
- [x] BUG-030, BUG-031, BUG-032, BUG-034 et redimensionnement par les bords en mode Workspace (2026-10-06).
- [ ] BUG-033 : vérifier multi-écrans / DPI mixte ; Snap layouts Windows 11 (HTMAXBUTTON).

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

## Outils de modification / dessin (ADR-018)

- [ ] BUG-026 — Supprimer les anciens modes Move3D / Copy3D / Rotate3D devenus inatteignables.
- [ ] Vérification manuelle GUI des 19 outils autres que Rectangle (Prolonger / Ajuster / Décaler en particulier).
- [ ] Étendre Symétrie aux treillis (mirrorElements ne les gère pas) ; saisie d'un axe de rotation autre que la
      normale du plan de travail en mode 3D.
- [ ] Accrochage « milieu de barre » et « perpendiculaire » pour la saisie des outils.

## Nettoyage du modèle

- [ ] Vérification manuelle GUI : « Nettoyer puis calculer » sur un vrai projet (nœuds confondus importés).
- [ ] Étendre aux câbles (extrémités confondues : déjà couvert par la fusion) et aux voiles / dalles (nœuds sur arêtes).
- [ ] Option : nettoyage limité à la sélection ; surbrillance des nœuds signalés « à vérifier ».

## Moteur 2D MetDeDeplacement (ADR-019)

- [ ] Vérification manuelle GUI : calcul Custom2D depuis la fenêtre Analysis sur un axe de grille, NDC affichée.
- [ ] BUG-027 — transmettre les relâchements d'extrémité à OpenSees (rotules de barre).
- [ ] Option : importer les anciens fichiers JSON « dessin » (legacy) dans le modèle TSA, si l'utilisateur le souhaite.
- [ ] Déformée viewport : exploiter les déplacements le long des barres (stations u, v) pour une déformée courbe.

## Analyse multi-moteurs (ADR-017)

- [ ] Persister `AnalysisContext` dans le .tsa (chunk dédié, version mineure) — BUG-013.
- [ ] Router Modal / Pushover du ruban par `runAnalysis` (après BUG-024 pour le pushover).
- [ ] Vérification manuelle GUI : calcul depuis la fenêtre Analysis (OpenSees, portée axe), panneau « Résultats
      d'analyse » des Propriétés, onglets masqués du dock « Données d'analyse ».
- [ ] BUG-024 — Implémenter réellement pushover / temporel dans le générateur OpenSees (ou retirer l'action).

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

## Miniatures Explorateur (ADR-016)

- [ ] Installeur (Inno Setup / WiX) : copier TSAThumbnailProvider.dll à côté de TSA.exe, `regsvr32 /i:machine /n`
      à l'installation, `/u /i:machine /n` à la désinstallation.
- [ ] Vérifier après redémarrage de Windows et dans une fenêtre Explorateur (Très grandes icônes).
- [ ] Signer numériquement TSA.exe et la DLL (confiance SmartScreen / antivirus).

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

- [x] Outils de modification / dessin en saisie 3D (fenêtre optionnelle), 20 outils ; correction de la copie (section affichée) (2026-10-05, tests 139–149).

- [x] Architecture d'analyse multi-moteurs : contexte, portée par grille/niveau/WorkPlane, AnalysisModel dérivé, mapping, registre, OpenSeesEngine, Custom2DEngine (adaptateur), fenêtre Analysis commune, résultats pilotés par capacités (2026-10-05, tests 130–138).

- [x] Extraction avancée OpenSees : mapping TSA↔OpenSees, U/R/forces local-global-basic, matrices élémentaires et K_global, export, contexte IA, dock ; corrections FIX-015 à FIX-022 (2026-10-04, tests 105–111, non commité).

- [x] Symétrie, division de barres, fusion de nœuds confondus + avertissement dalles/voiles non calculés (2026-10-04, tests 101–104, non commité).

- [x] Audit stabilité/performance + corrections (merge 5ed3cf9) — FIX-001 à FIX-008, FIX-014.
- [x] Système d'édition : invalidation des résultats, transactions, historique structuré, sélection ensembliste, règle des niveaux, budget mémoire (merge 81e34cf) — FIX-009 à FIX-013.
- [x] Initialisation de la mémoire technique `.claude/` (2026-10-03).
