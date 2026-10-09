# Tasks

Last Updated: 2026-10-07. Uniquement des tâches réellement identifiées (voir known-issues.md).

## BIM (ADR-020, docs/BIM_ARCHITECTURE.md)

- [x] Phases 1–3 : noyau BIM, identifiants stables, mapping physique → analytique, chunk BIMM (tests 170–176).
- [x] Export IFC 4.3 (STEP) + validation IfcOpenShell ; import IFC ; test aller-retour (tests 177–180).
- [ ] BUG-028 : charges / combinaisons IFC (IfcStructuralLoadGroup…), relâchements, unités dérivées, grilles.
- [ ] Repère de grille « B-3 » des produits (information BIM, GridManager hors Model).
- [ ] Validation BIM pré-calcul, sous-ensemble IDS, abstraction BCF, API IA contrôlée.
- [x] UI : actions Exporter / Importer IFC (essai GUI 2026-10-06).
- [ ] UI : propriétés BIM (GlobalId, catégorie, Psets) dans le panneau Propriétés.
- [x] BUG-029 : métadonnées BIM au collage (test 190, 2026-10-07).
- [ ] Docs restantes : BIM_GUIDELINES.md, AI_API.md, VERSIONING.md (IFC_MAPPING.md fait).

## Fenêtre / Start Center (ADR-021)

- [x] AppShell, barre de titre, Start Center, Nouveau projet, Fermer le projet (essais GUI 2026-10-06).
- [x] BUG-030, BUG-031, BUG-032, BUG-034 et redimensionnement par les bords en mode Workspace (2026-10-06).
- [x] Crash en réduisant la fenêtre agrandie (FIX-2026-10-07-MINIMIZE, 2026-10-07).
- [ ] BUG-033 : vérifier multi-écrans / DPI mixte ; Snap layouts Windows 11 (HTMAXBUTTON).

## Statique seul (ADR-022, branche feature/static-only)

- [x] Retrait du modal / pushover / temporel (code, UI, NDC, tests) ; archive `archive/dynamique` + tag `v0.2.0-dynamique` (locaux).
- [ ] Commit, fusion dans `main`, push (et push de `archive/dynamique` + tag) — sur accord de l'utilisateur.
- [ ] Vérifications GUI : ruban sans entrées dynamiques, calcul statique depuis F5, progression / Annuler.

## Écosystème Tsaraloha (ADR-024) — feuille de route

- [x] Phases 1–6 : bibliothèques partagées, ProjectSession, IDE TSALab, cœur scientifique, registre de commandes, Blueprint.
- [x] Fusion dans main (locale, 2026-10-08) : feature/tsaraloha-shared-libs (TSA) et feature/scientific-ide (TSALab). Push : sur demande.
- [x] Console de TSA : commandes du registre (identifiant à point) et COMMANDES (2026-10-08). Les raccourcis (FIT, ISO…) restent.
- [ ] Outils IA (AIToolRegistry) : outils de lecture adossés au registre de commandes.
- [x] Phase 6 : Blueprint — runtime typé, .tsbp, éditeur dans TSALab (2026-10-08).
- [x] Phase 7 : AnalysisController (session), AnalysisManagerPanel, K·U = F (Custom2D), espaces Analyse / Résultats /
      Recherche de TSALab, commandes analysis.run / results.* (2026-10-08). M et C : sans objet (statique seul, ADR-022).
- [ ] TSA : proposer l'AnalysisManagerPanel dans un dock (aujourd'hui fenêtre Analysis + ruban).
- [x] Phase 8 : débogueur Blueprint, tâche de fond, historique de l'éditeur, API de plugins, IA sur Blueprint (script,
      propose_blueprint), OpenSees dans tsalab_science (2026-10-08).
- [ ] Vérifier en GUI avec un modèle de langage configuré : propose_blueprint de bout en bout (TSA et TSALab).
- [ ] OpenSeesPlanarSolver : rotules et charges partielles / trapézoïdales (eleLoad partiel selon la version d'OpenSees).
- [ ] Plugins : signature / liste blanche des DLL avant diffusion hors poste de développement.
- [ ] BUG-036 (cache CMake). [x] BUG-037 corrigé.

## CRITICAL

- [ ] (aucune tâche critique ouverte)

## HIGH

- [ ] BUG-002 — Mailler dalles/voiles pour le calcul (l'avertissement explicite avant calcul est fait le 2026-10-04).
- [x] BUG-011 — lanceur généré à la page de code de la génération (2026-10-07, `ninja -t deps` vérifié).

## MEDIUM

- [x] BUG-001 — calcul en thread + progression annulable (code) ; [ ] vérifier en GUI : progression, Annuler, fermeture refusée pendant le calcul.
- [x] BUG-003 — niveaux / axes / grilles annulables (test 188) ; plans de travail volontairement hors historique.
- [x] BUG-004 / BUG-009 — isolation branchée, module mort supprimé ; [ ] vérifier en GUI (I, H, Alt+H, Ctrl+H, Alt+I, Alt+W).
- [x] BUG-005 — édition groupée (test 191) ; [ ] vérifier le panneau en GUI (sélection de 3 poutres, changer la section).
- [ ] Vérifications manuelles GUI non automatisées : enchaînement Z→X→Y, entrée/sortie mode 2D, sélection d'un câble fin, Undo après édition de propriété, message « résultats obsolètes ».
- [ ] Édition géométrique manquante : division à un nœud existant / aux intersections, symétrie par plan quelconque, ouvertures dalles/voiles, grips viewport.
- [ ] Vérification manuelle GUI : Symétrie, Diviser les barres, Fusionner les nœuds (menu Édition / ruban), Undo de chacune.

- [x] BUG-016 — stations exactes + déformée intégrée (tests 181–184).
- [ ] Vérification manuelle GUI : dock « Données d'analyse », configuration LIGHT/ADVANCED, export, charge sur poteau via MemberLoadDialog.

## Outils de modification / dessin (ADR-018)

- [x] BUG-026 — anciens modes Move3D / Copy3D / Rotate3D supprimés.
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
- [x] BUG-027 — rotules transmises à OpenSees (test 183).
- [ ] Option : importer les anciens fichiers JSON « dessin » (legacy) dans le modèle TSA, si l'utilisateur le souhaite.
- [ ] Déformée viewport : exploiter les déplacements le long des barres (stations u, v) pour une déformée courbe.

## Analyse multi-moteurs (ADR-017)

- [x] `AnalysisContext` persisté (chunk SETT, format 1.4, test 189).
- [x] Modal / Pushover retirés (ADR-022).
- [ ] Vérification manuelle GUI : calcul depuis la fenêtre Analysis (OpenSees, portée axe), panneau « Résultats
      d'analyse » des Propriétés, onglets masqués du dock « Données d'analyse ».
- [x] BUG-024 — retiré avec le dynamique (ADR-022).

## IA Co-Engineering (ADR-015)

- [x] Fournisseur Cloud « Anthropic Claude » (préréglage `anthropic`, point d'accès compatible OpenAI) — 2026-10-09.
- [ ] Vérifier le fournisseur Anthropic Claude en conditions réelles avec une clé API (console.anthropic.com) :
      discussion complète, appels d'outils en streaming ; la couche de compatibilité OpenAI est en bêta chez Anthropic.
- [x] Claude Code co-ingénieur par MCP (AutomationServer + tsaraloha-mcp.exe, docs/MCP.md) — 2026-10-09.
- [ ] MCP : enregistrer un projet / ouvrir un fichier (aujourd'hui : l'ingénieur enregistre), captures du viewport.
- [ ] What-If : `SimulationScenario` (copie du modèle via snapshot, calcul, comparaison, jamais d'écrasement).
- [ ] Rapport d'ingénierie IA (15 sections, citations des données TSA, réutiliser le moteur NDC).
- [ ] « Expliquer avec l'IA » depuis les docks Propriétés et Résultats (actuellement : arbre + ruban).
- [ ] Distribution : embarquer une build llama.cpp testée dans `<app>/ai/llama` (détectée en priorité) ;
      aujourd'hui : llama.cpp installé via winget.
- [ ] Évaluer la qualité des réponses avec Qwen3 4B/8B sur une machine non saturée (BUG-021).
- [x] BUG-020 — qtbase_fr déployée et chargée ; [ ] vérifier les boutons en GUI.

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

- [x] BUG-017 — équilibre en moments.
- [x] BUG-018 — liens NDC avec famille ; [ ] vérifier le clic en GUI.
- [x] BUG-019 — nœuds de treillis purs (test 184).
- [ ] Persistance optionnelle des résultats avancés (lié à BUG-013).

- [ ] BUG-006 — Ajouter des `EditRecord` aux appels directs de `pushUndoState` (dialogues, viewport).
- [x] BUG-007 — index id → item (test 166).
- [ ] BUG-008 — Mesurer l'ouverture en Release ; partager la géométrie des sphères de nœuds si nécessaire.
- [x] BUG-010 — `NCollection_HArray1<gp_Pnt>`.
- [ ] BUG-013 — résultats (RSLT) non persistés ; paramètres faits (SETT).
- [ ] BUG-035 — test 20 sensible au temps (médiane de 3 essais ou seuil Debug).
- [ ] Fichiers > 2 000 lignes (OccView_Navigation.cpp, OccView_Shapes.cpp, MainWindow_Actions.cpp) : analyse de découpage avant d'y ajouter des responsabilités.

## COMPLETED

- [x] Outils de modification / dessin en saisie 3D (fenêtre optionnelle), 20 outils ; correction de la copie (section affichée) (2026-10-05, tests 139–149).

- [x] Architecture d'analyse multi-moteurs : contexte, portée par grille/niveau/WorkPlane, AnalysisModel dérivé, mapping, registre, OpenSeesEngine, Custom2DEngine (adaptateur), fenêtre Analysis commune, résultats pilotés par capacités (2026-10-05, tests 130–138).

- [x] Extraction avancée OpenSees : mapping TSA↔OpenSees, U/R/forces local-global-basic, matrices élémentaires et K_global, export, contexte IA, dock ; corrections FIX-015 à FIX-022 (2026-10-04, tests 105–111, non commité).

- [x] Symétrie, division de barres, fusion de nœuds confondus + avertissement dalles/voiles non calculés (2026-10-04, tests 101–104, non commité).

- [x] Audit stabilité/performance + corrections (merge 5ed3cf9) — FIX-001 à FIX-008, FIX-014.
- [x] Système d'édition : invalidation des résultats, transactions, historique structuré, sélection ensembliste, règle des niveaux, budget mémoire (merge 81e34cf) — FIX-009 à FIX-013.
- [x] Initialisation de la mémoire technique `.claude/` (2026-10-03).
